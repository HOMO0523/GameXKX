#!/usr/bin/env python3
"""GameXXK DeepSeek vision integration (deepseek-v4-flash-vision-exp).

Pure-stdlib client so it runs both under system Python and inside the UE
embedded Python environment (no ``openai`` / ``requests`` dependency).

Protocol implemented here follows the official DeepSeek image-input contract:

* chat/completions with ``content`` block arrays;
* three image transports: base64 data URL, public http(s) URL, Files API
  ``file_id`` (plus the ``file_data`` inline variant);
* ``detail`` levels low / high / original / auto for ``image_url`` blocks
  (ignored for ``file`` blocks, as documented);
* Files API upload for reuse across requests.

Image format is always detected from actual bytes, never from the filename.
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import struct
import sys
import time
import urllib.error
import urllib.request
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable, Sequence

__all__ = [
    "DeepSeekVisionClient",
    "DeepSeekVisionError",
    "VisionResult",
    "build_image_block",
    "detect_image_format",
    "encode_data_url",
    "file_data_block",
    "file_id_block",
    "image_dimensions",
]

DEFAULT_BASE_URL = "https://api.deepseek.com"
DEFAULT_MODEL = "deepseek-v4-flash-vision-exp"
ENV_API_KEY = "DEEPSEEK_API_KEY"

SUPPORTED_FORMATS = ("jpeg", "png", "gif", "webp")
MIME_BY_FORMAT = {
    "jpeg": "image/jpeg",
    "png": "image/png",
    "gif": "image/gif",
    "webp": "image/webp",
}
EXTENSION_BY_FORMAT = {
    "jpeg": "jpg",
    "png": "png",
    "gif": "gif",
    "webp": "webp",
}
VALID_DETAILS = ("low", "high", "original", "auto")

# Limits from the DeepSeek image-input documentation.
MAX_URL_LENGTH = 8192
MAX_REQUEST_BODY_BYTES = 48 * 1024 * 1024
MAX_INLINE_IMAGE_BYTES = 32 * 1024 * 1024
MAX_FILE_ID_IMAGE_BYTES = 64 * 1024 * 1024
MAX_TOTAL_INLINE_IMAGE_BYTES = 64 * 1024 * 1024
MAX_IMAGES_PER_REQUEST = 600
MAX_IMAGE_SIDE_DEFAULT = 8192
MAX_IMAGE_SIDE_MANY = 4096
MANY_IMAGES_THRESHOLD = 15

DEFAULT_OCR_PROMPT = "识别这张图片里的全部文字，按原文顺序输出，不要添加解释或补充内容。"
DEFAULT_DESCRIBE_PROMPT = "请描述这张图片的内容。"
DEFAULT_UPLOAD_PURPOSE = "assistants"

_JPEG_SOF_MARKERS = frozenset(
    {
        0xC0,
        0xC1,
        0xC2,
        0xC3,
        0xC5,
        0xC6,
        0xC7,
        0xC9,
        0xCA,
        0xCB,
        0xCD,
        0xCE,
        0xCF,
    }
)


class DeepSeekVisionError(RuntimeError):
    """Raised for invalid input or a failed DeepSeek API call."""


@dataclass
class VisionResult:
    content: str
    model: str = ""
    finish_reason: str | None = None
    usage: dict[str, Any] = field(default_factory=dict)
    raw: dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        return {
            "content": self.content,
            "model": self.model,
            "finish_reason": self.finish_reason,
            "usage": dict(self.usage),
        }


@dataclass(frozen=True)
class _BlockInfo:
    block: dict[str, Any]
    inline_bytes: int = 0
    width: int | None = None
    height: int | None = None
    image_format: str | None = None


def detect_image_format(data: bytes) -> str:
    """Return one of ``jpeg`` / ``png`` / ``gif`` / ``webp`` from actual bytes."""
    if len(data) < 16:
        raise DeepSeekVisionError("image data too short to detect a supported format")
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        return "png"
    if data[:6] in (b"GIF87a", b"GIF89a"):
        return "gif"
    if data[:4] == b"RIFF" and len(data) >= 12 and data[8:12] == b"WEBP":
        return "webp"
    if data[:2] == b"\xff\xd8":
        return "jpeg"
    raise DeepSeekVisionError(
        "unsupported image format: only JPEG, PNG, GIF and WebP are accepted "
        "(format is detected from file content, not from the filename)"
    )


def _png_dimensions(data: bytes) -> tuple[int, int]:
    if len(data) < 24 or data[12:16] != b"IHDR":
        raise DeepSeekVisionError("cannot parse PNG dimensions: IHDR chunk missing")
    return struct.unpack(">II", data[16:24])


def _gif_dimensions(data: bytes) -> tuple[int, int]:
    if len(data) < 10:
        raise DeepSeekVisionError("cannot parse GIF dimensions: header too short")
    return struct.unpack("<HH", data[6:10])


def _jpeg_dimensions(data: bytes) -> tuple[int, int]:
    if len(data) < 4:
        raise DeepSeekVisionError("cannot parse JPEG dimensions: header too short")
    offset = 2
    while offset + 4 <= len(data):
        if data[offset] != 0xFF:
            offset += 1
            continue
        marker = data[offset + 1]
        if marker in (0xD8, 0x01) or 0xD0 <= marker <= 0xD7:
            offset += 2
            continue
        if marker in (0xD9, 0xDA):
            break
        if offset + 4 > len(data):
            break
        segment_length = int.from_bytes(data[offset + 2 : offset + 4], "big")
        if segment_length < 2 or offset + 2 + segment_length > len(data):
            break
        if marker in _JPEG_SOF_MARKERS:
            if segment_length < 7:
                break
            height = int.from_bytes(data[offset + 5 : offset + 7], "big")
            width = int.from_bytes(data[offset + 7 : offset + 9], "big")
            return width, height
        offset += 2 + segment_length
    raise DeepSeekVisionError("cannot parse JPEG dimensions: no SOF marker found")


def _webp_dimensions(data: bytes) -> tuple[int, int]:
    if len(data) < 30:
        raise DeepSeekVisionError("cannot parse WebP dimensions: header too short")
    kind = data[12:16]
    if kind == b"VP8 ":
        if data[20:23] != b"\x9d\x01\x2a":
            raise DeepSeekVisionError("cannot parse lossy WebP dimensions: invalid frame tag")
        width = int.from_bytes(data[26:28], "little") & 0x3FFF
        height = int.from_bytes(data[28:30], "little") & 0x3FFF
        return width, height
    if kind == b"VP8L":
        if data[20] != 0x2F:
            raise DeepSeekVisionError("cannot parse lossless WebP dimensions: invalid signature")
        bits = int.from_bytes(data[21:25], "little")
        return (bits & 0x3FFF) + 1, ((bits >> 14) & 0x3FFF) + 1
    if kind == b"VP8X":
        return (
            1 + int.from_bytes(data[24:27], "little"),
            1 + int.from_bytes(data[27:30], "little"),
        )
    raise DeepSeekVisionError("cannot parse WebP dimensions: unknown chunk layout")


def image_dimensions(data: bytes) -> tuple[int, int]:
    """Return ``(width, height)`` parsed from the actual image bytes."""
    image_format = detect_image_format(data)
    if image_format == "png":
        return _png_dimensions(data)
    if image_format == "gif":
        return _gif_dimensions(data)
    if image_format == "jpeg":
        return _jpeg_dimensions(data)
    return _webp_dimensions(data)


def encode_data_url(data: bytes) -> str:
    """Encode bytes as a ``data:<mime>;base64,...`` URL using detected content."""
    image_format = detect_image_format(data)
    encoded = base64.b64encode(data).decode("ascii")
    return f"data:{MIME_BY_FORMAT[image_format]};base64,{encoded}"


def _validate_inline_image_bytes(data: bytes) -> None:
    if len(data) > MAX_INLINE_IMAGE_BYTES:
        raise DeepSeekVisionError(
            f"inline image is {len(data)} bytes; single-image limit for base64 / URL "
            f"transport is {MAX_INLINE_IMAGE_BYTES} bytes ({MAX_INLINE_IMAGE_BYTES // (1024 * 1024)} MiB). "
            "Use the Files API for larger images (up to 64 MiB)."
        )
    detect_image_format(data)


def _validate_image_sides(width: int | None, height: int | None, image_count: int) -> None:
    if width is None or height is None:
        return
    max_side = MAX_IMAGE_SIDE_MANY if image_count >= MANY_IMAGES_THRESHOLD else MAX_IMAGE_SIDE_DEFAULT
    if max(width, height) > max_side:
        rule = (
            f"requests with >= {MANY_IMAGES_THRESHOLD} images allow at most {MAX_IMAGE_SIDE_MANY}px per side"
            if image_count >= MANY_IMAGES_THRESHOLD
            else f"single-image side limit is {MAX_IMAGE_SIDE_DEFAULT}px"
        )
        raise DeepSeekVisionError(
            f"image dimensions {width}x{height} exceed the limit: {rule}"
        )


def _normalize_detail(detail: str | None) -> str | None:
    if detail is None:
        return None
    normalized = str(detail).lower()
    if normalized not in VALID_DETAILS:
        raise DeepSeekVisionError(f"invalid detail {detail!r}; expected one of {VALID_DETAILS}")
    return normalized


def _image_url_block(url: str, detail: str | None) -> dict[str, Any]:
    if not isinstance(url, str) or not url:
        raise DeepSeekVisionError("image_url.url must be a non-empty string")
    # The 8192-character limit is documented for external http(s) URLs only;
    # base64 data URLs are constrained by the 48 MiB request body instead.
    if url.startswith(("http://", "https://")) and len(url) > MAX_URL_LENGTH:
        raise DeepSeekVisionError(
            f"external image URL is {len(url)} characters; limit is {MAX_URL_LENGTH}. "
            "Use a base64 data URL or the Files API for long links."
        )
    if not url.startswith(("data:", "http://", "https://")):
        raise DeepSeekVisionError("image URL must be a data:, http:// or https:// URL")
    image_url: dict[str, Any] = {"url": url}
    if detail is not None:
        image_url["detail"] = detail
    return {"type": "image_url", "image_url": image_url}


def file_id_block(file_id: str) -> dict[str, Any]:
    """Build a Files API ``file`` content block from a returned ``file_id``."""
    if not isinstance(file_id, str) or not file_id:
        raise DeepSeekVisionError("file_id must be a non-empty string")
    return {"type": "file", "file_id": file_id}


def file_data_block(data: bytes, filename: str | None = None) -> dict[str, Any]:
    """Build an inline ``file`` content block carrying base64 ``file_data``.

    ``file_data`` and ``file_id`` are mutually exclusive; this helper only
    produces the ``file_data`` variant.
    """
    _validate_inline_image_bytes(data)
    image_format = detect_image_format(data)
    resolved_filename = filename or f"image.{EXTENSION_BY_FORMAT[image_format]}"
    if not isinstance(resolved_filename, str) or not resolved_filename:
        raise DeepSeekVisionError("file filename must be a non-empty string")
    return {
        "type": "file",
        "file_data": encode_data_url(data),
        "filename": resolved_filename,
    }


def _bytes_input_block(data: bytes, detail: str | None) -> _BlockInfo:
    _validate_inline_image_bytes(data)
    image_format = detect_image_format(data)
    width, height = image_dimensions(data)
    return _BlockInfo(
        block=_image_url_block(encode_data_url(data), detail),
        inline_bytes=len(data),
        width=width,
        height=height,
        image_format=image_format,
    )


def _path_input_block(path: str | os.PathLike[str], detail: str | None) -> _BlockInfo:
    image_path = Path(path)
    if not image_path.is_file():
        raise DeepSeekVisionError(f"image file not found: {image_path}")
    try:
        data = image_path.read_bytes()
    except OSError as exc:
        raise DeepSeekVisionError(f"cannot read image file {image_path}: {exc}") from exc
    return _bytes_input_block(data, detail)


def _dict_input_block(raw: dict[str, Any], detail: str | None) -> _BlockInfo:
    block_type = raw.get("type")
    if block_type == "image_url":
        image_url = raw.get("image_url")
        if not isinstance(image_url, dict):
            raise DeepSeekVisionError("image_url content block requires an image_url object")
        url = image_url.get("url")
        if not isinstance(url, str) or not url:
            raise DeepSeekVisionError("image_url content block requires a non-empty url")
        merged = dict(raw)
        merged["image_url"] = dict(image_url)
        if detail is not None and "detail" not in merged["image_url"]:
            merged["image_url"]["detail"] = detail
        block_detail = _normalize_detail(merged["image_url"].get("detail"))
        if url.startswith("data:"):
            return _data_url_input_block(url, block_detail)
        return _BlockInfo(block=_image_url_block(url, block_detail))
    if block_type == "file":
        file_id = raw.get("file_id")
        file_data = raw.get("file_data")
        if (file_id is None) == (file_data is None):
            raise DeepSeekVisionError("file content block requires exactly one of file_id or file_data")
        merged = dict(raw)
        if file_id is not None:
            return _BlockInfo(block=file_id_block(file_id))
        if not isinstance(file_data, str) or not file_data.startswith("data:"):
            raise DeepSeekVisionError("file_data must be a base64 data: URL")
        try:
            payload = base64.b64decode(file_data.split(",", 1)[1], validate=True)
        except Exception as exc:
            raise DeepSeekVisionError("file_data is not valid base64") from exc
        _validate_inline_image_bytes(payload)
        filename = merged.get("filename")
        if not isinstance(filename, str) or not filename:
            raise DeepSeekVisionError("file content blocks with file_data require a filename")
        width, height = image_dimensions(payload)
        return _BlockInfo(
            block=merged,
            inline_bytes=len(payload),
            width=width,
            height=height,
            image_format=detect_image_format(payload),
        )
    raise DeepSeekVisionError(
        "unsupported content block type; expected image_url or file"
    )


def _data_url_input_block(url: str, detail: str | None) -> _BlockInfo:
    header, separator, encoded = url.partition(",")
    if not separator or not header.startswith("data:") or ";base64" not in header:
        raise DeepSeekVisionError("data: image URL must be a base64 data URL")
    try:
        data = base64.b64decode(encoded, validate=True)
    except Exception as exc:
        raise DeepSeekVisionError("data: image URL is not valid base64") from exc
    _validate_inline_image_bytes(data)
    image_format = detect_image_format(data)
    width, height = image_dimensions(data)
    return _BlockInfo(
        block=_image_url_block(url, detail),
        inline_bytes=len(data),
        width=width,
        height=height,
        image_format=image_format,
    )


def _url_input_block(url: str, detail: str | None) -> _BlockInfo:
    if url.startswith("data:"):
        return _data_url_input_block(url, detail)
    return _BlockInfo(block=_image_url_block(url, detail))


def build_image_block(
    source: str | os.PathLike[str] | bytes | dict[str, Any],
    detail: str | None = None,
) -> dict[str, Any]:
    """Normalize one image input into an OpenAI-compatible content block.

    * ``bytes`` -> base64 data URL ``image_url`` block (format detected from content);
    * local path -> base64 data URL ``image_url`` block;
    * ``http(s)://`` / ``data:`` string -> external ``image_url`` block;
    * ``dict`` -> validated passthrough for ``image_url`` / ``file`` blocks.
    """
    normalized_detail = _normalize_detail(detail)
    if isinstance(source, dict):
        return _dict_input_block(source, normalized_detail).block
    if isinstance(source, bytes):
        return _bytes_input_block(source, normalized_detail).block
    if isinstance(source, (str, os.PathLike)):
        text = os.fspath(source)
        if text.startswith(("http://", "https://", "data:")):
            return _url_input_block(text, normalized_detail).block
        return _path_input_block(text, normalized_detail).block
    raise DeepSeekVisionError(
        f"unsupported image input type: {type(source).__name__}; "
        "expected bytes, a local path, an http(s)/data URL string, or a content block dict"
    )


def _build_block_infos(
    images: Iterable[str | os.PathLike[str] | bytes | dict[str, Any]],
    urls: Iterable[str],
    file_ids: Iterable[str],
    detail: str | None,
) -> list[_BlockInfo]:
    normalized_detail = _normalize_detail(detail)
    infos: list[_BlockInfo] = []
    for image in images:
        if isinstance(image, dict):
            infos.append(_dict_input_block(image, normalized_detail))
        elif isinstance(image, bytes):
            infos.append(_bytes_input_block(image, normalized_detail))
        elif isinstance(image, (str, os.PathLike)):
            text = os.fspath(image)
            if text.startswith(("http://", "https://", "data:")):
                infos.append(_url_input_block(text, normalized_detail))
            else:
                infos.append(_path_input_block(text, normalized_detail))
        else:
            raise DeepSeekVisionError(
                f"unsupported image input type: {type(image).__name__}"
            )
    for url in urls:
        infos.append(_url_input_block(url, normalized_detail))
    for file_id in file_ids:
        infos.append(_BlockInfo(block=file_id_block(file_id)))
    return infos


def _validate_block_infos(infos: list[_BlockInfo]) -> None:
    if not infos:
        raise DeepSeekVisionError("at least one image is required")
    if len(infos) > MAX_IMAGES_PER_REQUEST:
        raise DeepSeekVisionError(
            f"{len(infos)} images requested; limit is {MAX_IMAGES_PER_REQUEST} per request"
        )
    total_inline = sum(info.inline_bytes for info in infos)
    if total_inline > MAX_TOTAL_INLINE_IMAGE_BYTES:
        raise DeepSeekVisionError(
            f"inline image total is {total_inline} bytes; requests without Files API "
            f"file_id images allow at most {MAX_TOTAL_INLINE_IMAGE_BYTES} bytes "
            f"({MAX_TOTAL_INLINE_IMAGE_BYTES // (1024 * 1024)} MiB)"
        )
    for info in infos:
        _validate_image_sides(info.width, info.height, len(infos))


class DeepSeekVisionClient:
    """Minimal stdlib client for DeepSeek chat/completions image inputs."""

    def __init__(
        self,
        api_key: str | None = None,
        *,
        base_url: str = DEFAULT_BASE_URL,
        model: str = DEFAULT_MODEL,
        timeout: float = 120.0,
        max_retries: int = 3,
    ) -> None:
        self.api_key = (api_key or os.environ.get(ENV_API_KEY) or "").strip()
        if not self.api_key:
            raise DeepSeekVisionError(
                f"missing DeepSeek API key: pass api_key= or set the {ENV_API_KEY} environment variable"
            )
        self.base_url = (base_url or DEFAULT_BASE_URL).rstrip("/")
        if not self.base_url:
            raise DeepSeekVisionError("base_url must not be empty")
        self.model = model or DEFAULT_MODEL
        self.timeout = float(timeout)
        self.max_retries = max(0, int(max_retries))

    # ------------------------------------------------------------------ #
    # Request building
    # ------------------------------------------------------------------ #
    def build_chat_payload(
        self,
        prompt: str,
        *,
        images: Iterable[str | os.PathLike[str] | bytes | dict[str, Any]] = (),
        urls: Iterable[str] = (),
        file_ids: Iterable[str] = (),
        detail: str | None = None,
        system: str | None = None,
        max_tokens: int | None = None,
        temperature: float | None = None,
    ) -> dict[str, Any]:
        if not isinstance(prompt, str) or not prompt.strip():
            raise DeepSeekVisionError("prompt must be a non-empty string")
        infos = _build_block_infos(images, urls, file_ids, detail)
        _validate_block_infos(infos)

        content: list[dict[str, Any]] = [{"type": "text", "text": prompt}]
        content.extend(info.block for info in infos)
        messages: list[dict[str, Any]] = []
        if system:
            messages.append({"role": "system", "content": system})
        messages.append({"role": "user", "content": content})

        payload: dict[str, Any] = {"model": self.model, "messages": messages}
        if max_tokens is not None:
            payload["max_tokens"] = int(max_tokens)
        if temperature is not None:
            payload["temperature"] = float(temperature)
        return payload

    # ------------------------------------------------------------------ #
    # Transport
    # ------------------------------------------------------------------ #
    def _request_headers(self, extra: dict[str, str] | None = None) -> dict[str, str]:
        headers = {"Authorization": f"Bearer {self.api_key}"}
        if extra:
            headers.update(extra)
        return headers

    def _post(
        self,
        path: str,
        payload: dict[str, Any],
        *,
        headers: dict[str, str] | None = None,
    ) -> dict[str, Any]:
        url = f"{self.base_url}{path}"
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        if len(body) > MAX_REQUEST_BODY_BYTES:
            raise DeepSeekVisionError(
                f"request body is {len(body)} bytes; limit is {MAX_REQUEST_BODY_BYTES} bytes "
                f"({MAX_REQUEST_BODY_BYTES // (1024 * 1024)} MiB)"
            )
        last_error: DeepSeekVisionError | None = None
        for attempt in range(self.max_retries + 1):
            request = urllib.request.Request(
                url,
                data=body,
                headers={**self._request_headers({"Content-Type": "application/json"}), **(headers or {})},
                method="POST",
            )
            try:
                with urllib.request.urlopen(request, timeout=self.timeout) as response:
                    raw = response.read()
                    if not isinstance(raw, bytes):
                        raw = bytes(raw)
                    status = getattr(response, "status", 200)
                    parsed = self._decode_json_body(raw)
                    if 200 <= status < 300:
                        return parsed
                    raise DeepSeekVisionError(self._api_error_message(parsed, raw, status))
            except urllib.error.HTTPError as exc:
                status = int(exc.code)
                try:
                    parsed = self._decode_json_body(exc.read())
                except Exception:
                    parsed = {}
                message = self._api_error_message(parsed, b"", status)
                if not self._is_retryable(status):
                    raise DeepSeekVisionError(message) from exc
                last_error = DeepSeekVisionError(message)
                self._sleep_before_retry(attempt, exc.headers)
            except (urllib.error.URLError, TimeoutError, OSError) as exc:
                last_error = DeepSeekVisionError(f"DeepSeek request failed: {exc}")
                self._sleep_before_retry(attempt, None)
        raise last_error or DeepSeekVisionError("DeepSeek request failed")

    @staticmethod
    def _decode_json_body(raw: bytes) -> dict[str, Any]:
        text = raw.decode("utf-8", "ignore").strip()
        if not text:
            return {}
        parsed = json.loads(text)
        if not isinstance(parsed, dict):
            raise DeepSeekVisionError("DeepSeek API returned a non-object JSON response")
        return parsed

    @staticmethod
    def _api_error_message(parsed: dict[str, Any], raw: bytes, status: int) -> str:
        error = parsed.get("error")
        if isinstance(error, dict):
            message = error.get("message")
            if isinstance(message, str) and message:
                return f"DeepSeek API error HTTP {status}: {message}"
        if isinstance(error, str) and error:
            return f"DeepSeek API error HTTP {status}: {error}"
        preview = raw.decode("utf-8", "ignore")[:500] if raw else ""
        suffix = f": {preview}" if preview else ""
        return f"DeepSeek API error HTTP {status}{suffix}"

    @staticmethod
    def _is_retryable(status: int) -> bool:
        return status in (408, 409, 429) or status >= 500

    @staticmethod
    def _sleep_before_retry(attempt: int, headers: Any) -> None:
        retry_after: float | None = None
        if headers is not None:
            value = headers.get("Retry-After") or headers.get("retry-after")
            if value:
                try:
                    retry_after = float(value)
                except (TypeError, ValueError):
                    retry_after = None
        if retry_after is not None:
            time.sleep(min(max(retry_after, 0.0), 10.0))
        else:
            time.sleep(min(0.5 * (2**attempt), 8.0))

    # ------------------------------------------------------------------ #
    # chat/completions
    # ------------------------------------------------------------------ #
    def analyze(
        self,
        prompt: str,
        *,
        images: Iterable[str | os.PathLike[str] | bytes | dict[str, Any]] = (),
        urls: Iterable[str] = (),
        file_ids: Iterable[str] = (),
        detail: str | None = None,
        system: str | None = None,
        max_tokens: int | None = None,
        temperature: float | None = None,
    ) -> VisionResult:
        payload = self.build_chat_payload(
            prompt,
            images=images,
            urls=urls,
            file_ids=file_ids,
            detail=detail,
            system=system,
            max_tokens=max_tokens,
            temperature=temperature,
        )
        response = self._post("/chat/completions", payload)
        return self._parse_chat_response(response)

    @staticmethod
    def _parse_chat_response(payload: dict[str, Any]) -> VisionResult:
        choices = payload.get("choices")
        if not isinstance(choices, list) or not choices:
            raise DeepSeekVisionError("DeepSeek response has no choices")
        first = choices[0]
        if not isinstance(first, dict):
            raise DeepSeekVisionError("DeepSeek response choice is malformed")
        message = first.get("message")
        if not isinstance(message, dict):
            raise DeepSeekVisionError("DeepSeek response choice has no message")
        content = message.get("content")
        if isinstance(content, list):
            parts: list[str] = []
            for item in content:
                if isinstance(item, dict) and isinstance(item.get("text"), str):
                    parts.append(item["text"])
            content = "\n".join(parts)
        if not isinstance(content, str):
            content = ""
        return VisionResult(
            content=content,
            model=payload.get("model") or "",
            finish_reason=first.get("finish_reason"),
            usage=payload.get("usage") if isinstance(payload.get("usage"), dict) else {},
            raw=payload,
        )

    def describe(
        self,
        *images: str | os.PathLike[str] | bytes | dict[str, Any],
        prompt: str = DEFAULT_DESCRIBE_PROMPT,
        **kwargs: Any,
    ) -> VisionResult:
        return self.analyze(prompt, images=images, **kwargs)

    def ocr(
        self,
        *images: str | os.PathLike[str] | bytes | dict[str, Any],
        prompt: str = DEFAULT_OCR_PROMPT,
        **kwargs: Any,
    ) -> VisionResult:
        return self.analyze(prompt, images=images, **kwargs)

    # ------------------------------------------------------------------ #
    # Files API
    # ------------------------------------------------------------------ #
    def upload_file(
        self,
        source: str | os.PathLike[str] | bytes,
        *,
        filename: str | None = None,
        purpose: str = DEFAULT_UPLOAD_PURPOSE,
    ) -> dict[str, Any]:
        if isinstance(source, bytes):
            data = source
            if filename is None:
                image_format = detect_image_format(data)
                filename = f"image.{EXTENSION_BY_FORMAT[image_format]}"
        else:
            image_path = Path(source)
            if not image_path.is_file():
                raise DeepSeekVisionError(f"upload file not found: {image_path}")
            try:
                data = image_path.read_bytes()
            except OSError as exc:
                raise DeepSeekVisionError(f"cannot read upload file {image_path}: {exc}") from exc
            if filename is None:
                filename = image_path.name
        if not isinstance(filename, str) or not filename:
            raise DeepSeekVisionError("upload filename must be a non-empty string")
        detect_image_format(data)
        if len(data) > MAX_FILE_ID_IMAGE_BYTES:
            raise DeepSeekVisionError(
                f"upload image is {len(data)} bytes; Files API limit is "
                f"{MAX_FILE_ID_IMAGE_BYTES} bytes ({MAX_FILE_ID_IMAGE_BYTES // (1024 * 1024)} MiB)"
            )

        boundary = f"----GameXXKDeepSeek{uuid.uuid4().hex}"
        parts: list[bytes] = []
        parts.append(
            (
                f"--{boundary}\r\n"
                f'Content-Disposition: form-data; name="purpose"\r\n\r\n'
                f"{purpose}\r\n"
            ).encode("utf-8")
        )
        parts.append(
            (
                f"--{boundary}\r\n"
                f'Content-Disposition: form-data; name="file"; filename="{filename}"\r\n'
                f"Content-Type: application/octet-stream\r\n\r\n"
            ).encode("utf-8")
        )
        parts.append(data)
        parts.append(f"\r\n--{boundary}--\r\n".encode("utf-8"))
        body = b"".join(parts)

        response = self._post_multipart("/files", body, boundary)
        file_id = response.get("id")
        if not isinstance(file_id, str) or not file_id:
            raise DeepSeekVisionError(
                f"Files API response has no file id: {json.dumps(response, ensure_ascii=False)[:500]}"
            )
        return response

    def _post_multipart(self, path: str, body: bytes, boundary: str) -> dict[str, Any]:
        url = f"{self.base_url}{path}"
        last_error: DeepSeekVisionError | None = None
        for attempt in range(self.max_retries + 1):
            request = urllib.request.Request(
                url,
                data=body,
                headers=self._request_headers(
                    {"Content-Type": f"multipart/form-data; boundary={boundary}"}
                ),
                method="POST",
            )
            try:
                with urllib.request.urlopen(request, timeout=self.timeout) as response:
                    raw = response.read()
                    if not isinstance(raw, bytes):
                        raw = bytes(raw)
                    return self._decode_json_body(raw)
            except urllib.error.HTTPError as exc:
                status = int(exc.code)
                try:
                    parsed = self._decode_json_body(exc.read())
                except Exception:
                    parsed = {}
                message = self._api_error_message(parsed, b"", status)
                if not self._is_retryable(status):
                    raise DeepSeekVisionError(message) from exc
                last_error = DeepSeekVisionError(message)
                self._sleep_before_retry(attempt, exc.headers)
            except (urllib.error.URLError, TimeoutError, OSError) as exc:
                last_error = DeepSeekVisionError(f"DeepSeek Files API request failed: {exc}")
                self._sleep_before_retry(attempt, None)
        raise last_error or DeepSeekVisionError("DeepSeek Files API request failed")


# ---------------------------------------------------------------------- #
# CLI
# ---------------------------------------------------------------------- #
def _atomic_write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f"{path.name}.{time.time_ns()}.tmp")
    try:
        temporary.write_text(text, encoding="utf-8")
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def _client_from_args(args: argparse.Namespace, *, require_key: bool = True) -> DeepSeekVisionClient | None:
    if require_key:
        return DeepSeekVisionClient(
            api_key=args.api_key,
            base_url=args.base_url,
            model=args.model,
            timeout=args.timeout,
            max_retries=args.max_retries,
        )
    api_key = (args.api_key or os.environ.get(ENV_API_KEY) or "").strip() or "dry-run"
    return DeepSeekVisionClient(
        api_key=api_key,
        base_url=args.base_url,
        model=args.model,
        timeout=args.timeout,
        max_retries=args.max_retries,
    )


def _run_analyze(args: argparse.Namespace) -> int:
    images: list[Any] = list(args.image or [])
    urls: list[str] = list(args.url or [])
    file_ids: list[str] = list(args.file_id or [])
    prompt = args.prompt or args.default_prompt
    client = _client_from_args(args, require_key=not args.dry_run)
    assert client is not None
    payload = client.build_chat_payload(
        prompt,
        images=images,
        urls=urls,
        file_ids=file_ids,
        detail=args.detail,
        system=args.system,
        max_tokens=args.max_tokens,
        temperature=args.temperature,
    )
    if args.dry_run:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
        return 0
    result = client._parse_chat_response(client._post("/chat/completions", payload))
    _write_result(args, result)
    return 0


def _run_upload(args: argparse.Namespace) -> int:
    if args.dry_run:
        data: bytes
        if args.source == "-":
            data = sys.stdin.buffer.read()
        else:
            data = Path(args.source).read_bytes()
        image_format = detect_image_format(data)
        filename = args.filename or ("stdin.png" if args.source == "-" else Path(args.source).name)
        print(
            json.dumps(
                {
                    "endpoint": f"{args.base_url.rstrip('/')}/files",
                    "method": "POST",
                    "purpose": args.purpose,
                    "filename": filename,
                    "bytes": len(data),
                    "format": image_format,
                    "note": "dry-run; no network request was made",
                },
                ensure_ascii=False,
                indent=2,
            )
        )
        return 0
    client = _client_from_args(args, require_key=True)
    assert client is not None
    source: str | os.PathLike[str] | bytes
    if args.source == "-":
        source = sys.stdin.buffer.read()
    else:
        source = args.source
    response = client.upload_file(source, filename=args.filename, purpose=args.purpose)
    rendered = json.dumps(response, ensure_ascii=False, indent=2)
    if args.json:
        print(rendered)
    else:
        print(rendered)
    if args.output:
        _atomic_write_text(Path(args.output), rendered + "\n")
    return 0


def _write_result(args: argparse.Namespace, result: VisionResult) -> None:
    rendered = json.dumps(result.to_dict(), ensure_ascii=False, indent=2)
    if args.json:
        print(rendered)
    else:
        print(result.content)
    if args.output:
        _atomic_write_text(Path(args.output), (rendered if args.json else result.content) + "\n")


def _add_common_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--model", default=DEFAULT_MODEL, help=f"vision model (default: {DEFAULT_MODEL})")
    parser.add_argument("--base-url", default=os.environ.get("DEEPSEEK_BASE_URL", DEFAULT_BASE_URL))
    parser.add_argument("--api-key", default=None, help=f"defaults to ${ENV_API_KEY}")
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--max-retries", type=int, default=3)
    parser.add_argument("--detail", choices=VALID_DETAILS, default=None, help="image detail level for image_url blocks")
    parser.add_argument("--max-tokens", type=int, default=None)
    parser.add_argument("--temperature", type=float, default=None)
    parser.add_argument("--system", default=None, help="optional system message")
    parser.add_argument("--json", action="store_true", help="print machine-readable JSON")
    parser.add_argument("--dry-run", action="store_true", help="validate and print the request payload without calling the API")
    parser.add_argument("-o", "--output", default=None, help="write result to file")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="GameXXK DeepSeek vision helper (deepseek-v4-flash-vision-exp)."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    describe = subparsers.add_parser("describe", help="describe one or more images")
    describe.add_argument("image", nargs="+", help="local image path, data: URL, or http(s) URL")
    describe.add_argument("--prompt", default=None)
    describe.add_argument("--url", action="append", default=None, help="public image URL; repeatable")
    describe.add_argument("--file-id", action="append", default=None, help="Files API file_id; repeatable")
    describe.set_defaults(default_prompt=DEFAULT_DESCRIBE_PROMPT, handler=_run_analyze)
    _add_common_arguments(describe)

    ocr = subparsers.add_parser("ocr", help="extract text from one or more images")
    ocr.add_argument("image", nargs="+", help="local image path, data: URL, or http(s) URL")
    ocr.add_argument("--prompt", default=None)
    ocr.add_argument("--url", action="append", default=None, help="public image URL; repeatable")
    ocr.add_argument("--file-id", action="append", default=None, help="Files API file_id; repeatable")
    ocr.set_defaults(default_prompt=DEFAULT_OCR_PROMPT, handler=_run_analyze)
    _add_common_arguments(ocr)

    analyze = subparsers.add_parser("analyze", help="analyze arbitrary image mixes with one prompt")
    _add_common_arguments(analyze)
    _add_block_arguments(analyze)
    analyze.add_argument("--prompt", required=True)
    analyze.set_defaults(default_prompt="", handler=_run_analyze)

    upload = subparsers.add_parser("upload", help="upload an image through the Files API and print the file_id")
    upload.add_argument("source", help="local image path or '-' for stdin")
    upload.add_argument("--purpose", default=DEFAULT_UPLOAD_PURPOSE)
    upload.add_argument("--filename", default=None)
    upload.set_defaults(handler=_run_upload)
    _add_common_arguments(upload)

    return parser


def _add_block_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--image", action="append", default=None, help="image path / data: / http(s) URL; repeatable")
    parser.add_argument("--url", action="append", default=None, help="public image URL; repeatable")
    parser.add_argument("--file-id", action="append", default=None, help="Files API file_id; repeatable")


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8")
    try:
        handler = getattr(args, "handler", None)
        if handler is None:
            parser.error("missing command")
        return int(handler(args))
    except (DeepSeekVisionError, OSError) as exc:
        if getattr(args, "json", False):
            print(json.dumps({"ok": False, "error": str(exc)}, ensure_ascii=False))
        else:
            print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
