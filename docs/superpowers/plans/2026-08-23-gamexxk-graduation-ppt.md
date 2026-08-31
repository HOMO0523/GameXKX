# GameXXK Graduation PPT Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a real 50-slide 16:9 `.pptx` that updates the old WPS format with current GameXXK gameplay, HTML interfaces, Slate architecture, numeric-testing system, and all 173 active cards.

**Architecture:** One PptxGenJS source file builds the complete deck from local screenshots and the generated card catalog. PowerPoint COM exports the deck to PDF and PNG for deterministic page-count, text, image, and visual review.

**Tech Stack:** PptxGenJS, Sharp, Playwright with installed Chrome, Microsoft PowerPoint COM, local GameXXK screenshots and Markdown data.

---

### Task 1: Prepare current visual evidence

**Files:**
- Create: `Deliverables/GameXXK_Graduation_Presentation/assets/html-cover.png`
- Create: `Deliverables/GameXXK_Graduation_Presentation/assets/html-slate.png`
- Create: `Deliverables/GameXXK_Graduation_Presentation/assets/html-numeric-tests.png`
- Create: `Deliverables/GameXXK_Graduation_Presentation/assets/html-card-table.png`

- [ ] Capture the local HTML at 1920×1080 with installed Chrome.
- [ ] Capture `#top`, `#slate`, `#numbers`, and `#role-partner-guard` after scrolling each anchor to the top.
- [ ] Copy the current town, workbench, route, battle, and SafeStage screenshots into the same asset directory.
- [ ] Verify every PNG opens, has nonzero dimensions, and records its SHA256.

### Task 2: Generate the 50-slide deck

**Files:**
- Create: `scripts/build_gamexxk_graduation_ppt.js`
- Create: `Deliverables/GameXXK_Graduation_Presentation/GameXXK_毕业设计答辩.pptx`

- [ ] Define a 16:9 custom theme using paper `#E8D9BE`, paper highlight `#F3E8D2`, ink `#2D2A24`, yellow highlight `#F2D84B`, cinnabar `#A74635`, pine `#526E65`, and wood `#6B4A2D`.
- [ ] Implement shared title, section, screenshot, architecture, metric, and six-card table layouts with fixed page number/footer placement.
- [ ] Write slides 1–20 from the approved PPT design specification and current HTML content.
- [ ] Parse `docs/design/2026-08-11-full-card-catalog.md`, keep the 173 active cards, assert bucket counts `36+108+24+5`, and generate slides 21–49 with six cards per slide.
- [ ] Write slide 50 with sources, version boundaries, HTML/PPT paths, and the historical-simulation limitation.
- [ ] Assert the deck contains exactly 50 slides before writing the `.pptx`.

### Task 3: Export and validate

**Files:**
- Create: `Deliverables/GameXXK_Graduation_Presentation/GameXXK_毕业设计答辩.pdf`
- Create: `Saved/HarnessReports/gamexxk-graduation-ppt/slides/`
- Create: `Saved/HarnessReports/gamexxk-graduation-ppt/manifest.json`

- [ ] Open the generated PPTX through PowerPoint COM and export PDF plus 1920×1080 PNG slides.
- [ ] Verify 50 PNGs, 50 PowerPoint slides, 173 unique CardIds, no non-boss route cards, and no external image relationships.
- [ ] Build contact sheets for slides 1–20 and 21–50.
- [ ] Review the cover, three-core slide, Slate slide, numeric-test slide, and representative Hero/Guard/NPC card slides with a suitable method.
- [ ] Fix only confirmed overflow, clipping, contrast, alignment, or image-crop problems; regenerate and re-export once.

### Task 4: Deliver

**Files:**
- Create: `Deliverables/GameXXK_Graduation_Presentation/使用说明.txt`
- Create: `Deliverables/GameXXK_Graduation_Presentation.zip`

- [ ] Record that the deck is 16:9, 50 pages, uses 173 active cards, and cites the old WPS only as format/iteration reference.
- [ ] Zip the PPTX, PDF, and instructions without including source scripts or temporary slide PNGs.
- [ ] Return clickable links to the PPTX, PDF, folder, and ZIP.
