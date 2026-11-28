#!/usr/bin/env python3
"""Build the formatted project report from the generated experiment CSVs."""

import csv
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.shared import Inches, Pt, RGBColor
from docx.oxml.ns import qn


ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results"
FONT = "/System/Library/Fonts/Supplemental/Arial.ttf"
FONT_BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
POLICIES = ["fifo", "lru", "lfu", "custom"]
WORKLOADS = ["sequential", "repetitive", "mixed"]
COLORS = {
    "fifo": "#2563eb",
    "lru": "#16a34a",
    "lfu": "#ea580c",
    "custom": "#9333ea",
    "sequential": "#2563eb",
    "repetitive": "#ea580c",
    "mixed": "#16a34a",
}


def rows(path):
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def font(size, bold=False):
    return ImageFont.truetype(FONT_BOLD if bold else FONT, size)


def draw_text(draw, xy, text, size=22, color="#111827", anchor=None, bold=False):
    draw.text(xy, str(text), font=font(size, bold), fill=color, anchor=anchor)


def create_workload_chart():
    distribution = rows(RESULTS / "resource_frequency.csv")
    width, height = 1500, 720
    image = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(image)
    draw_text(draw, (width // 2, 35), "Top-resource request counts by workload",
              32, anchor="mm", bold=True)
    names = {
        workload: sorted(
            [r for r in distribution if r["workload"] == workload],
            key=lambda r: int(r["request_count"]),
            reverse=True,
        )[:5]
        for workload in WORKLOADS
    }
    maximum = max(int(r["request_count"]) for group in names.values() for r in group)
    top, bottom, left, right = 100, 610, 125, 1450
    y_max = ((maximum + 99) // 100) * 100
    for tick in range(0, y_max + 1, 100):
        y = bottom - tick / y_max * (bottom - top)
        draw.line((left, y, right, y), fill="#d1d5db", width=2)
        draw_text(draw, (left - 15, y), tick, 18, color="#4b5563", anchor="rm")
    group_width = (right - left) / 5
    bar_width = 72
    for rank in range(5):
        center = left + group_width * (rank + 0.5)
        draw_text(draw, (center, bottom + 32), f"Rank {rank + 1}",
                  20, color="#374151", anchor="mm")
        for wi, workload in enumerate(WORKLOADS):
            value = int(names[workload][rank]["request_count"])
            x = center + (wi - 1) * (bar_width + 14)
            y = bottom - value / y_max * (bottom - top)
            draw.rectangle((x - bar_width/2, y, x + bar_width/2, bottom),
                           fill=COLORS[workload])
            draw_text(draw, (x, y - 8), value, 16, anchor="ms")
    for i, workload in enumerate(WORKLOADS):
        x = 490 + i * 220
        draw.rectangle((x, 70, x + 24, 94), fill=COLORS[workload])
        draw_text(draw, (x + 35, 82), workload.title(), 20, anchor="lm")
    draw_text(draw, (35, (top + bottom) // 2), "Requests",
              20, color="#374151")
    image.save(RESULTS / "report_workload_profiles.png")


def create_capacity_chart():
    data = rows(RESULTS / "experiment_summary.csv")
    labels = ["100 KiB", "500 KiB", "1 MiB", "2 MiB", "5 MiB", "10 MiB"]
    width, height = 1500, 1050
    image = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(image)
    draw_text(draw, (width // 2, 35), "Cache hit ratio by cache capacity",
              32, anchor="mm", bold=True)
    for pi, policy in enumerate(POLICIES):
        x = 240 + pi * 260
        draw.line((x, 78, x + 55, 78), fill=COLORS[policy], width=5)
        draw_text(draw, (x + 68, 78), policy.upper(), 20, anchor="lm")
    left, right = 145, 1450
    panel_h = 220
    for wi, workload in enumerate(WORKLOADS):
        top = 125 + wi * 310
        bottom = top + panel_h
        draw_text(draw, (width//2, top - 22), workload.title(),
                  22, anchor="mm", bold=True)
        for tick in range(0, 101, 20):
            y = bottom - tick / 100 * panel_h
            draw.line((left, y, right, y), fill="#d1d5db", width=2)
            draw_text(draw, (left - 14, y), f"{tick}%", 16,
                      color="#4b5563", anchor="rm")
        x_positions = [
            left + i * (right - left) / (len(labels) - 1)
            for i in range(len(labels))
        ]
        for x, label in zip(x_positions, labels):
            draw_text(draw, (x, bottom + 24), label, 17,
                      color="#374151", anchor="mm")
        for policy in POLICIES:
            values = [
                float(next(
                    r for r in data
                    if r["workload"] == workload
                    and r["capacity_label"] == label
                    and r["policy"] == policy
                )["hit_ratio_percent"])
                for label in labels
            ]
            points = [
                (x_positions[i], bottom - values[i] / 100 * panel_h)
                for i in range(len(labels))
            ]
            draw.line(points, fill=COLORS[policy], width=5)
            for x, y in points:
                draw.ellipse((x-7, y-7, x+7, y+7), fill=COLORS[policy])
    image.save(RESULTS / "report_hit_ratio_vs_capacity.png")


def create_client_chart():
    data = rows(RESULTS / "mixed_clients_5MiB.csv")
    names = sorted({r["client_id"] for r in data})
    width, height = 1500, 740
    image = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(image)
    draw_text(draw, (width//2, 38),
              "Mixed workload: per-client hit ratio at 5 MiB",
              30, anchor="mm", bold=True)
    left, right, top, bottom = 145, 1450, 125, 625
    for i, policy in enumerate(POLICIES):
        x = 430 + i * 190
        draw.rectangle((x, 77, x+24, 101), fill=COLORS[policy])
        draw_text(draw, (x+35, 89), policy.upper(), 20, anchor="lm")
    for tick in range(0, 101, 20):
        y = bottom - tick / 100 * (bottom - top)
        draw.line((left, y, right, y), fill="#d1d5db", width=2)
        draw_text(draw, (left-14, y), f"{tick}%", 16,
                  color="#4b5563", anchor="rm")
    group_width = (right - left) / len(names)
    bar_width = 34
    for ni, name in enumerate(names):
        center = left + group_width * (ni + 0.5)
        draw_text(draw, (center, bottom + 32), name, 17,
                  color="#374151", anchor="mm")
        for pi, policy in enumerate(POLICIES):
            value = float(next(r for r in data
                               if r["client_id"] == name
                               and r["policy"] == policy)["hit_ratio_percent"])
            x = center + (pi - 1.5) * (bar_width + 6)
            y = bottom - value / 100 * (bottom - top)
            draw.rectangle((x-bar_width/2, y, x+bar_width/2, bottom),
                           fill=COLORS[policy])
    image.save(RESULTS / "report_mixed_clients.png")


def set_cell_shading(cell, fill):
    from docx.oxml import OxmlElement
    from docx.oxml.ns import qn
    shading = OxmlElement("w:shd")
    shading.set(qn("w:fill"), fill)
    cell._tc.get_or_add_tcPr().append(shading)


def format_table(table, header=True, size=8):
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.style = "Table Grid"
    for ri, row in enumerate(table.rows):
        for cell in row.cells:
            cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
            for paragraph in cell.paragraphs:
                paragraph.paragraph_format.space_after = Pt(1)
                paragraph.paragraph_format.space_before = Pt(1)
                for run in paragraph.runs:
                    run.font.name = "Arial"
                    run.font.size = Pt(size)
                    run.font.color.rgb = RGBColor(31, 41, 55)
                    if header and ri == 0:
                        run.bold = True
                        run.font.color.rgb = RGBColor(255, 255, 255)
            if header and ri == 0:
                set_cell_shading(cell, "1F2937")


def add_table(document, headers, rows_data, size=8):
    table = document.add_table(rows=1, cols=len(headers))
    for cell, value in zip(table.rows[0].cells, headers):
        cell.text = str(value)
    for row_data in rows_data:
        cells = table.add_row().cells
        for cell, value in zip(cells, row_data):
            cell.text = str(value)
    format_table(table, True, size)
    return table


def set_doc_styles(document):
    section = document.sections[0]
    section.top_margin = Inches(0.58)
    section.bottom_margin = Inches(0.58)
    section.left_margin = Inches(0.65)
    section.right_margin = Inches(0.65)
    normal = document.styles["Normal"]
    normal.font.name = "Arial"
    normal.font.size = Pt(9)
    normal.font.color.rgb = RGBColor(31, 41, 55)
    normal.paragraph_format.space_after = Pt(5)
    normal.paragraph_format.line_spacing = 1.05
    for style_name, size in [("Heading 1", 15), ("Heading 2", 11)]:
        style = document.styles[style_name]
        style.font.name = "Arial"
        style.font.size = Pt(size)
        style.font.bold = True
        style.font.color.rgb = RGBColor(0, 0, 0)
        style.paragraph_format.space_before = Pt(7)
        style.paragraph_format.space_after = Pt(4)
        style.paragraph_format.keep_with_next = True
    title = document.styles["Title"]
    title.font.name = "Arial"
    title.font.size = Pt(24)
    title.font.bold = True
    title.font.color.rgb = RGBColor(0, 0, 0)
    title.paragraph_format.space_after = Pt(3)
    title_ppr = title._element.get_or_add_pPr()
    title_border = title_ppr.find(qn("w:pBdr"))
    if title_border is not None:
        title_ppr.remove(title_border)


def add_caption(document, text):
    p = document.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(2)
    p.paragraph_format.space_after = Pt(5)
    run = p.add_run(text)
    run.italic = True
    run.font.size = Pt(8)
    run.font.color.rgb = RGBColor(75, 85, 99)


def build_docx():
    create_workload_chart()
    create_capacity_chart()
    create_client_chart()
    document = Document()
    set_doc_styles(document)

    p = document.add_paragraph(style="Title")
    p.add_run("Shared Web Cache Simulator")
    p = document.add_paragraph()
    p.paragraph_format.space_after = Pt(12)
    run = p.add_run("Experimental Report")
    run.font.name = "Arial"
    run.font.size = Pt(13)
    run.font.color.rgb = RGBColor(75, 85, 99)

    document.add_paragraph(
        "The custom policy produced the highest request hit ratio and the "
        "lowest average response time at 5 MiB in all three supplied "
        "workloads. FIFO generated the least origin traffic in the mixed "
        "workload, showing that the best policy depends on the metric."
    )
    document.add_heading("Simulation design", level=1)
    document.add_paragraph(
        "The C simulator processes HTTP requests in trace order using a "
        "single shared cache. It stores resource metadata rather than real "
        "web content and does not contact origin servers. Cache hits take "
        "5 ms. Misses use the resource latency from resources.csv. The cache "
        "starts empty; expiration is disabled; all requests are treated as "
        "cacheable. Resources larger than the cache are fetched from origin "
        "but never stored."
    )
    add_table(document, ["Policy", "Victim rule"], [
        ["FIFO", "Remove the resource inserted earliest."],
        ["LRU", "Remove the resource used least recently."],
        ["LFU", "Remove the least-used resource; break ties by LRU."],
        ["Custom", "Remove the resource with the lowest retention score."],
    ], 8)
    document.add_heading("Reference trace validation", level=1)
    document.add_paragraph(
        "The 15-request reference trace was run with a 100 KiB cache. "
        "Automated regression checks compare every output row, including "
        "hits, evictions, cache contents, usage, response time, and "
        "cumulative totals."
    )
    add_table(document, ["Policy", "Hits", "Hit ratio", "Origin traffic", "Avg. response"], [
        ["FIFO", "3 / 15", "20.00%", "412 KiB", "95.33 ms"],
        ["LRU", "3 / 15", "20.00%", "394 KiB", "92.33 ms"],
        ["LFU", "4 / 15", "26.67%", "362 KiB", "84.33 ms"],
    ], 8)

    document.add_page_break()
    document.add_heading("Workloads and experiment method", level=1)
    document.add_paragraph(
        "Each workload has 3,000 requests, six clients, and 21 distinct "
        "resources. Popularity differs substantially, while large media "
        "files account for a large share of requested bytes."
    )
    add_table(document,
        ["Workload", "Requests", "Resources", "Clients", "Requested data"],
        [
            ["Sequential", "3,000", "21", "6", "15.84 GB"],
            ["Repetitive", "3,000", "21", "6", "4.38 GB"],
            ["Mixed", "3,000", "21", "6", "14.04 GB"],
        ], 8)
    document.add_picture(str(RESULTS / "report_workload_profiles.png"),
                         width=Inches(6.35))
    add_caption(document, "Figure 1. Request counts for the five most popular resources.")
    document.add_paragraph(
        "The sequential trace spreads its requests relatively evenly. The "
        "repetitive trace is dominated by four resources: lecture 08 (680 "
        "requests), the logo (666), app.js (630), and the course index (629). "
        "The mixed trace combines repeated web assets, documents, and media."
    )
    document.add_paragraph(
        "The experiment runner tests FIFO, LRU, LFU, and custom at 100 KiB, "
        "500 KiB, 1 MiB, 2 MiB, 5 MiB, and 10 MiB. At 5 MiB, the number of "
        "oversized requests is 288 sequential, 50 repetitive, and 234 mixed."
    )

    document.add_page_break()
    document.add_heading("Policy results at 5 MiB", level=1)
    document.add_paragraph(
        "The table compares request hits, byte hits, response time, origin "
        "traffic, and evictions. The experiment CSV also reports cache "
        "utilization and oversized requests."
    )
    summary = rows(RESULTS / "experiment_summary.csv")
    table_rows = []
    for workload in WORKLOADS:
        for policy in POLICIES:
            r = next(row for row in summary
                     if row["workload"] == workload
                     and row["capacity_label"] == "5 MiB"
                     and row["policy"] == policy)
            table_rows.append([
                workload.title(), policy.upper(),
                f'{float(r["hit_ratio_percent"]):.2f}%',
                f'{float(r["byte_hit_ratio_percent"]):.2f}%',
                f'{float(r["average_response_time_ms"]):.1f}',
                f'{int(r["origin_bytes"]) / 1048576:.1f}',
                r["evictions"],
            ])
    add_table(document,
        ["Workload", "Policy", "Hits", "Byte hits", "Avg ms", "Origin MiB", "Evictions"],
        table_rows, 7)
    document.add_paragraph(
        "Custom leads in request hit ratio and average response time in each "
        "workload. Repetition helps every policy, so the gaps are smaller in "
        "that trace. In the mixed trace, FIFO has the highest byte-hit ratio "
        "(5.19%) and lowest origin traffic (12,693.19 MiB), while custom has "
        "the highest request hit ratio (84.57%) and fastest average response "
        "(170.76 ms). Request hits count every file equally; byte hits weight "
        "large resources more heavily."
    )
    document.add_paragraph(
        "At 5 MiB, average utilization ranges from 69.87% to 84.15% for "
        "sequential, 77.59% to 82.35% for repetitive, and 77.49% to 83.96% "
        "for mixed. Oversized request counts are 288, 50, and 234 respectively."
    )
    document.add_picture(str(RESULTS / "report_hit_ratio_vs_capacity.png"),
                         width=Inches(5.9))
    add_caption(document, "Figure 2. Request hit ratio as cache capacity increases.")

    document.add_page_break()
    document.add_heading("Capacity and client effects", level=1)
    document.add_picture(str(RESULTS / "report_mixed_clients.png"),
                         width=Inches(6.35))
    add_caption(document, "Figure 3. Per-client hit ratios for the mixed workload at 5 MiB.")
    document.add_paragraph(
        "Increasing the cache from 5 MiB to 10 MiB raises the hit ratio for "
        "all workloads. At 10 MiB, the four policies converge: hit ratios "
        "are 89.77% sequential, 97.70% repetitive, and 91.57% mixed. The "
        "video resources still exceed 10 MiB and remain uncached."
    )
    document.add_paragraph(
        "At 5 MiB on the mixed trace, custom lowers overall average response "
        "time compared with LFU (170.76 ms versus 184.69 ms), but the gain "
        "does not reach every client. Students 01 and 02 have slightly higher "
        "average response times under custom (81.56 and 53.39 ms) than under "
        "LFU (73.84 and 50.60 ms). The other four clients improve."
    )
    document.add_heading("Custom policy and limitations", level=1)
    document.add_paragraph(
        "The custom retention score multiplies origin latency per KiB by a "
        "logarithmic frequency factor, a client-diversity bonus, and a "
        "recency factor. The lowest score is evicted. The client sketch is a "
        "64-bit hash approximation and can collide. The score weights are "
        "specific to this project; different traces may favor other choices."
    )
    document.add_paragraph(
        "No policy can cache the oversized videos under the tested capacities. "
        "The best policy depends on whether the goal is more request hits, "
        "more bytes served from cache, lower response time, or lower origin "
        "traffic. The CSV outputs and scripts allow the results to be "
        "reproduced and extended."
    )
    document.save(ROOT / "REPORT.docx")


if __name__ == "__main__":
    build_docx()
