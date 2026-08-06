#!/usr/bin/env python3
"""Merge the latest Standalone UI layout into the state embedded by CMake."""

from __future__ import annotations

import argparse
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


JUCE_STATE_MAGIC = 0x21324356
JUCE_MEMORY_ALPHABET = ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"


def decode_juce_memory_block(encoded: str) -> bytes:
    size_text, separator, payload = encoded.partition(".")
    if not separator or not size_text.isdigit():
        raise ValueError("invalid JUCE MemoryBlock encoding")

    result = bytearray(int(size_text))
    for position, character in enumerate(payload):
        value = JUCE_MEMORY_ALPHABET.find(character)
        if value < 0:
            continue
        bit_start = position * 6
        for bit in range(6):
            destination = bit_start + bit
            if destination >= len(result) * 8:
                break
            if value & (1 << bit):
                result[destination // 8] |= 1 << (destination % 8)
    return bytes(result)


def decode_state_xml(data: bytes) -> str:
    if len(data) < 9:
        raise ValueError("state file is too short")
    magic, xml_size = struct.unpack_from("<II", data)
    if magic != JUCE_STATE_MAGIC or xml_size > len(data) - 8:
        raise ValueError("invalid JUCE plug-in state header")
    return data[8 : 8 + xml_size].decode("utf-8")


def encode_state_xml(xml_text: str) -> bytes:
    xml_data = xml_text.encode("utf-8")
    return struct.pack("<II", JUCE_STATE_MAGIC, len(xml_data)) + xml_data + b"\0"


def extract_layout(xml_text: str) -> str:
    root = ET.fromstring(xml_text)
    layout = root if root.tag == "UILayout" else root.find("UILayout")
    if layout is None:
        raise ValueError("the state does not contain a UILayout")
    return ET.tostring(layout, encoding="unicode", short_empty_elements=True)


def replace_layout(state_xml: str, layout_xml: str) -> str:
    start = state_xml.find("<UILayout")
    closing = "</UILayout>"
    end = state_xml.find(closing, start)
    if start < 0 or end < 0:
        raise ValueError("the embedded state does not contain a UILayout")
    return state_xml[:start] + layout_xml + state_xml[end + len(closing) :]


def layout_from_settings(settings_file: Path) -> str:
    settings = ET.parse(settings_file).getroot()
    for value in settings.findall("VALUE"):
        if value.get("name") == "filterState":
            encoded = value.get("val", "")
            return extract_layout(decode_state_xml(decode_juce_memory_block(encoded)))
    raise ValueError("filterState was not found in the Standalone settings")


def default_candidates() -> list[tuple[Path, str]]:
    application_support = Path.home() / "Library" / "Application Support"
    return [
        (application_support / "NDLR" / "current-layout.xml", "layout cache"),
        (application_support / "NDLR.settings", "Standalone state"),
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--state", required=True, type=Path)
    arguments = parser.parse_args()

    state_file = arguments.state.resolve()
    state_data = state_file.read_bytes()
    state_xml = decode_state_xml(state_data)
    embedded_layout = extract_layout(state_xml)

    candidates = [
        (path, description)
        for path, description in default_candidates()
        if path.is_file() and path.stat().st_mtime > state_file.stat().st_mtime
    ]
    if not candidates:
        print("NDLR layout: embedded state is already the newest source")
        return 0

    source, description = max(candidates, key=lambda candidate: candidate[0].stat().st_mtime)
    try:
        latest_layout = (
            extract_layout(source.read_text(encoding="utf-8"))
            if description == "layout cache"
            else layout_from_settings(source)
        )
    except (ET.ParseError, OSError, UnicodeError, ValueError) as error:
        print(f"NDLR layout: ignoring invalid {description}: {error}", file=sys.stderr)
        return 0

    if latest_layout == embedded_layout:
        print(f"NDLR layout: {description} matches the embedded layout")
        return 0

    merged_xml = replace_layout(state_xml, latest_layout)
    ET.fromstring(merged_xml)
    state_file.write_bytes(encode_state_xml(merged_xml))
    print(f"NDLR layout: updated {state_file.name} from {description}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
