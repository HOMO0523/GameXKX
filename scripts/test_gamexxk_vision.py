"""Unit tests for the GameXXK DeepSeek vision client (no network access)."""

import contextlib
import io
import json
import os
import struct
import sys
import tempfile
import unittest
import urllib.error
from pathlib import Path
from unittest import mock

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import gamexxk_vision as vision  # noqa: E402


def _png(width: int, height: int, extra_padding: int = 0) -> bytes:
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + struct.pack(">I", len(ihdr))
        + b"IHDR"
        + ihdr
        + b"\x00\x00\x00\x00IEND"
        + b"\x00" * extra_padding
    )


def _gif(width: int, height: int) -> bytes:
    return b"GIF89a" + struct.pack("<HH", width, height) + b"\x00" * 16


def _jpeg(width: int, height: int) -> bytes:
    app0 = (
        b"\xff\xe0"
        + struct.pack(">H", 16)
        + b"JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00"
    )
    sof = (
        b"\xff\xc0"
        + struct.pack(">H", 17)
        + b"\x08"
        + struct.pack(">HH", height, width)
        + b"\x03"
        + b"\x01\x22\x00"
        + b"\x02\x11\x01"
        + b"\x03\x11\x01"
    )
    return b"\xff\xd8" + app0 + sof + b"\xff\xd9"


def _webp(width: int, height: int) -> bytes:
    data = bytearray(b"\x00" * 30)
    data[0:4] = b"RIFF"
    data[4:8] = struct.pack("<I", 22)
    data[8:12] = b"WEBP"
    data[12:16] = b"VP8 "
    data[20:23] = b"\x9d\x01\x2a"
    data[26:28] = struct.pack("<H", width & 0x3FFF)
    data[28:30] = struct.pack("<H", height & 0x3FFF)
    return bytes(data)


def _ok_response(payload: dict) -> dict:
    return json.dumps(
        {
            "id": "chatcmpl-test",
            "model": vision.DEFAULT_MODEL,
            "choices": [
                {
                    "index": 0,
                    "message": {"role": "assistant", "content": payload.get("content", "分析结果")},
                    "finish_reason": "stop",
                }
            ],
            "usage": {"prompt_tokens": 1, "completion_tokens": 2, "total_tokens": 3},
        }
    ).encode("utf-8")


class FakeResponse:
    def __init__(self, body: bytes, status: int = 200) -> None:
        self.body = body
        self.status = status

    def __enter__(self) -> "FakeResponse":
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        return None

    def read(self) -> bytes:
        return self.body


class FormatDetectionTests(unittest.TestCase):
    def test_detects_all_supported_formats_from_content(self) -> None:
        cases = {
            "png": _png(2, 2),
            "gif": _gif(3, 4),
            "jpeg": _jpeg(5, 6),
            "webp": _webp(7, 8),
        }
        for expected, data in cases.items():
            with self.subTest(expected=expected):
                self.assertEqual(vision.detect_image_format(data), expected)

    def test_rejects_unsupported_content(self) -> None:
        with self.assertRaisesRegex(vision.DeepSeekVisionError, "unsupported image format"):
            vision.detect_image_format(b"not-an-image-at-all" * 4)

    def test_mime_follows_actual_bytes_not_filename(self) -> None:
        data = _png(4, 4)
        url = vision.encode_data_url(data)
        self.assertTrue(url.startswith("data:image/png;base64,"))


class DimensionTests(unittest.TestCase):
    def test_png_dimensions(self) -> None:
        self.assertEqual(vision.image_dimensions(_png(640, 480)), (640, 480))

    def test_gif_dimensions(self) -> None:
        self.assertEqual(vision.image_dimensions(_gif(320, 200)), (320, 200))

    def test_jpeg_dimensions(self) -> None:
        self.assertEqual(vision.image_dimensions(_jpeg(1920, 1080)), (1920, 1080))

    def test_webp_dimensions(self) -> None:
        self.assertEqual(vision.image_dimensions(_webp(800, 600)), (800, 600))


class BlockBuildingTests(unittest.TestCase):
    def test_bytes_input_builds_base64_data_url_block(self) -> None:
        block = vision.build_image_block(_png(16, 16))
        self.assertEqual(block["type"], "image_url")
        self.assertTrue(block["image_url"]["url"].startswith("data:image/png;base64,"))

    def test_path_input_detects_format_from_content(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "actually-a-png.jpg"
            path.write_bytes(_png(8, 8))
            block = vision.build_image_block(path, detail="low")
            self.assertEqual(block["image_url"]["detail"], "low")
            self.assertTrue(block["image_url"]["url"].startswith("data:image/png;base64,"))

    def test_http_url_block_carries_detail(self) -> None:
        block = vision.build_image_block("https://example.com/a.png", detail="original")
        self.assertEqual(
            block,
            {
                "type": "image_url",
                "image_url": {"url": "https://example.com/a.png", "detail": "original"},
            },
        )

    def test_data_url_block_is_base64_validated(self) -> None:
        block = vision.build_image_block(vision.encode_data_url(_png(4, 4)))
        self.assertEqual(block["type"], "image_url")
        with self.assertRaisesRegex(vision.DeepSeekVisionError, "not valid base64"):
            vision.build_image_block("data:image/png;base64,%%%")

    def test_file_id_block(self) -> None:
        self.assertEqual(vision.file_id_block("file-api-abc"), {"type": "file", "file_id": "file-api-abc"})

    def test_file_data_block_uses_detected_extension(self) -> None:
        block = vision.file_data_block(_png(4, 4))
        self.assertEqual(block["type"], "file")
        self.assertEqual(block["filename"], "image.png")
        self.assertTrue(block["file_data"].startswith("data:image/png;base64,"))

    def test_content_block_dict_passthrough(self) -> None:
        raw = {"type": "file", "file_id": "file-api-xyz"}
        self.assertEqual(vision.build_image_block(raw), raw)
        raw_url = {
            "type": "image_url",
            "image_url": {"url": "https://example.com/b.png"},
        }
        block = vision.build_image_block(raw_url, detail="low")
        self.assertEqual(block["image_url"]["detail"], "low")

    def test_rejects_invalid_detail(self) -> None:
        with self.assertRaisesRegex(vision.DeepSeekVisionError, "invalid detail"):
            vision.build_image_block(_png(4, 4), detail="medium")

    def test_rejects_missing_file(self) -> None:
        with self.assertRaisesRegex(vision.DeepSeekVisionError, "not found"):
            vision.build_image_block(Path("definitely-missing-image.png"))


class PayloadBuildingTests(unittest.TestCase):
    def make_client(self, **kwargs) -> vision.DeepSeekVisionClient:
        return vision.DeepSeekVisionClient(api_key="test-key", **kwargs)

    def test_analyze_payload_mixes_all_three_transports(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            local = Path(directory) / "shot.png"
            local.write_bytes(_png(16, 16))
            client = self.make_client()
            payload = client.build_chat_payload(
                "对比这些图",
                images=[local],
                urls=["https://example.com/a.png"],
                file_ids=["file-api-1"],
                detail="low",
                system="你是视觉验收员",
                max_tokens=512,
            )
            self.assertEqual(payload["model"], vision.DEFAULT_MODEL)
            self.assertEqual(payload["max_tokens"], 512)
            self.assertEqual(payload["messages"][0], {"role": "system", "content": "你是视觉验收员"})
            content = payload["messages"][1]["content"]
            self.assertEqual([block["type"] for block in content], ["text", "image_url", "image_url", "file"])
            self.assertTrue(content[1]["image_url"]["url"].startswith("data:image/png;base64,"))
            self.assertEqual(content[1]["image_url"]["detail"], "low")
            self.assertEqual(content[2]["image_url"], {"url": "https://example.com/a.png", "detail": "low"})
            self.assertEqual(content[3], {"type": "file", "file_id": "file-api-1"})

    def test_detail_is_ignored_for_file_blocks(self) -> None:
        client = self.make_client()
        payload = client.build_chat_payload("看图", file_ids=["file-api-2"], detail="low")
        file_block = payload["messages"][0]["content"][1]
        self.assertNotIn("detail", file_block)

    def test_requires_at_least_one_image(self) -> None:
        client = self.make_client()
        with self.assertRaisesRegex(vision.DeepSeekVisionError, "at least one image"):
            client.build_chat_payload("没有图")

    def test_rejects_urls_over_length_limit(self) -> None:
        client = self.make_client()
        with mock.patch.object(vision, "MAX_URL_LENGTH", 10):
            with self.assertRaisesRegex(vision.DeepSeekVisionError, "limit is 10"):
                client.build_chat_payload("看图", urls=["https://example.com/long-url"])

    def test_url_length_limit_does_not_apply_to_base64_data_urls(self) -> None:
        # DeepSeek documents the 8192-character limit for external URLs only;
        # data URLs are governed by the request-body limit instead.
        data = _png(4, 4, extra_padding=4096)
        client = self.make_client()
        with mock.patch.object(vision, "MAX_URL_LENGTH", 10):
            payload = client.build_chat_payload("看图", images=[data])
        data_url = payload["messages"][0]["content"][1]["image_url"]["url"]
        self.assertTrue(len(data_url) > 10)
        self.assertTrue(data_url.startswith("data:image/png;base64,"))

    def test_rejects_more_than_max_images(self) -> None:
        client = self.make_client()
        with mock.patch.object(vision, "MAX_IMAGES_PER_REQUEST", 2):
            with self.assertRaisesRegex(vision.DeepSeekVisionError, "limit is 2"):
                client.build_chat_payload("看图", file_ids=["file-api-a", "file-api-b", "file-api-c"])

    def test_rejects_inline_image_over_32_mib_class_limit(self) -> None:
        client = self.make_client()
        with mock.patch.object(vision, "MAX_INLINE_IMAGE_BYTES", 10):
            with self.assertRaisesRegex(vision.DeepSeekVisionError, "single-image limit"):
                client.build_chat_payload("看图", images=[_png(4, 4)])

    def test_rejects_total_inline_over_limit(self) -> None:
        client = self.make_client()
        with mock.patch.object(vision, "MAX_TOTAL_INLINE_IMAGE_BYTES", 100):
            images = [_png(4, 4, extra_padding=60), _png(4, 4, extra_padding=60)]
            with self.assertRaisesRegex(vision.DeepSeekVisionError, "inline image total"):
                client.build_chat_payload("看图", images=images)

    def test_rejects_oversized_side_for_single_image(self) -> None:
        client = self.make_client()
        with mock.patch.object(vision, "MAX_IMAGE_SIDE_DEFAULT", 4):
            with self.assertRaisesRegex(vision.DeepSeekVisionError, "exceed the limit"):
                client.build_chat_payload("看图", images=[_png(8, 8)])

    def test_many_image_requests_use_4096_side_limit(self) -> None:
        client = self.make_client()
        with mock.patch.object(vision, "MANY_IMAGES_THRESHOLD", 2):
            with mock.patch.object(vision, "MAX_IMAGE_SIDE_MANY", 4):
                with self.assertRaisesRegex(vision.DeepSeekVisionError, "at most 4px"):
                    client.build_chat_payload("看图", images=[_png(8, 8), _png(8, 8)])

    def test_rejects_ambiguous_file_block(self) -> None:
        client = self.make_client()
        with self.assertRaisesRegex(vision.DeepSeekVisionError, "exactly one"):
            client.build_chat_payload("看图", images=[{"type": "file", "file_id": "a", "file_data": "data:image/png;base64,x"}])

    def test_file_data_block_requires_filename(self) -> None:
        client = self.make_client()
        raw = {"type": "file", "file_data": vision.encode_data_url(_png(4, 4))}
        with self.assertRaisesRegex(vision.DeepSeekVisionError, "require a filename"):
            client.build_chat_payload("看图", images=[raw])


class ClientTransportTests(unittest.TestCase):
    def make_client(self, max_retries: int = 0) -> vision.DeepSeekVisionClient:
        return vision.DeepSeekVisionClient(api_key="test-key", max_retries=max_retries)

    def test_missing_api_key_fails_fast(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True):
            with self.assertRaisesRegex(vision.DeepSeekVisionError, "missing DeepSeek API key"):
                vision.DeepSeekVisionClient()

    def test_api_key_falls_back_to_environment(self) -> None:
        with mock.patch.dict(os.environ, {"DEEPSEEK_API_KEY": "env-key"}, clear=True):
            client = vision.DeepSeekVisionClient()
            self.assertEqual(client.api_key, "env-key")

    def test_analyze_parses_content_and_usage(self) -> None:
        client = self.make_client()
        body = _ok_response({"content": "图里有一座桥"})
        with mock.patch("urllib.request.urlopen", return_value=FakeResponse(body)) as mocked:
            result = client.analyze("图里有什么？", images=[_png(4, 4)])
        self.assertEqual(result.content, "图里有一座桥")
        self.assertEqual(result.finish_reason, "stop")
        self.assertEqual(result.usage["total_tokens"], 3)
        request = mocked.call_args.args[0]
        self.assertEqual(request.full_url, f"{vision.DEFAULT_BASE_URL}/chat/completions")
        self.assertEqual(request.get_header("Authorization"), "Bearer test-key")
        sent = json.loads(request.data.decode("utf-8"))
        self.assertEqual(sent["model"], vision.DEFAULT_MODEL)

    def test_http_error_surfaces_api_message(self) -> None:
        client = self.make_client()
        error_body = json.dumps({"error": {"message": "This model does not support image"}}).encode("utf-8")
        http_error = urllib.error.HTTPError(
            "https://api.deepseek.com/chat/completions",
            400,
            "Bad Request",
            {},
            io.BytesIO(error_body),
        )
        with mock.patch("urllib.request.urlopen", side_effect=http_error):
            with self.assertRaisesRegex(vision.DeepSeekVisionError, "This model does not support image"):
                client.analyze("看图", images=[_png(4, 4)])

    def test_retries_transient_500_then_succeeds(self) -> None:
        client = self.make_client(max_retries=1)
        http_error = urllib.error.HTTPError(
            "https://api.deepseek.com/chat/completions",
            500,
            "Internal Server Error",
            {},
            io.BytesIO(b'{"error":{"message":"upstream"}}'),
        )
        with mock.patch(
            "urllib.request.urlopen",
            side_effect=[http_error, FakeResponse(_ok_response({"content": "ok"}))],
        ) as mocked, mock.patch.object(vision.time, "sleep") as sleep:
            result = client.analyze("看图", images=[_png(4, 4)])
        self.assertEqual(result.content, "ok")
        self.assertEqual(mocked.call_count, 2)
        sleep.assert_called_once()

    def test_request_body_limit_is_enforced_before_sending(self) -> None:
        client = self.make_client()
        with mock.patch.object(vision, "MAX_REQUEST_BODY_BYTES", 64):
            with mock.patch("urllib.request.urlopen") as mocked:
                with self.assertRaisesRegex(vision.DeepSeekVisionError, "request body is"):
                    client.analyze("看图", images=[_png(4, 4)])
        mocked.assert_not_called()

    def test_upload_file_posts_multipart_and_returns_id(self) -> None:
        client = self.make_client()
        upload_response = json.dumps({"id": "file-api-uploaded", "bytes": 37, "filename": "shot.png"}).encode("utf-8")
        with mock.patch("urllib.request.urlopen", return_value=FakeResponse(upload_response)) as mocked:
            response = client.upload_file(_png(4, 4))
        self.assertEqual(response["id"], "file-api-uploaded")
        request = mocked.call_args.args[0]
        self.assertEqual(request.full_url, f"{vision.DEFAULT_BASE_URL}/files")
        content_type = request.get_header("Content-type")
        self.assertIn("multipart/form-data; boundary=", content_type)
        self.assertIn(b'name="purpose"', request.data)
        self.assertIn(b'name="file"; filename="image.png"', request.data)

    def test_upload_file_rejects_unsupported_format(self) -> None:
        client = self.make_client()
        with self.assertRaisesRegex(vision.DeepSeekVisionError, "unsupported image format"):
            client.upload_file(b"definitely-not-an-image-data")

    def test_upload_file_rejects_over_64_mib(self) -> None:
        client = self.make_client()
        with mock.patch.object(vision, "MAX_FILE_ID_IMAGE_BYTES", 10):
            with self.assertRaisesRegex(vision.DeepSeekVisionError, "Files API limit"):
                client.upload_file(_png(4, 4))

    def test_parse_response_rejects_empty_choices(self) -> None:
        with self.assertRaisesRegex(vision.DeepSeekVisionError, "no choices"):
            vision.DeepSeekVisionClient._parse_chat_response({"choices": []})


class CliTests(unittest.TestCase):
    def test_dry_run_prints_request_without_key_or_network(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "shot.png"
            path.write_bytes(_png(16, 16))
            output = io.StringIO()
            with mock.patch.dict(os.environ, {}, clear=True):
                with contextlib.redirect_stdout(output):
                    code = vision.main(["describe", str(path), "--dry-run", "--detail", "low"])
            self.assertEqual(code, 0)
            rendered = json.loads(output.getvalue())
            self.assertEqual(rendered["model"], vision.DEFAULT_MODEL)
            blocks = rendered["messages"][0]["content"]
            self.assertEqual(blocks[0]["type"], "text")
            self.assertEqual(blocks[1]["image_url"]["detail"], "low")

    def test_cli_reports_validation_error_as_json(self) -> None:
        output = io.StringIO()
        with mock.patch.dict(os.environ, {}, clear=True), contextlib.redirect_stdout(output):
            code = vision.main(
                ["analyze", "--prompt", "看图", "--url", "x" * 100, "--json", "--dry-run"]
            )
        self.assertEqual(code, 2)
        rendered = json.loads(output.getvalue())
        self.assertFalse(rendered["ok"])
        self.assertIn("URL", rendered["error"])

    def test_cli_missing_key_exit_code(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "shot.png"
            path.write_bytes(_png(4, 4))
            with mock.patch.dict(os.environ, {}, clear=True), mock.patch("urllib.request.urlopen") as mocked:
                code = vision.main(["ocr", str(path)])
        self.assertEqual(code, 2)
        mocked.assert_not_called()


if __name__ == "__main__":
    unittest.main()
