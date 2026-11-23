"""Parse the supplied PADS-style netlist, without reading legacy firmware."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def parse(text: str) -> dict:
    sections: dict[str, list[str]] = {}
    current = ""
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("$"):
            current = line
            sections.setdefault(current, [])
        elif line:
            sections.setdefault(current, []).append(line)

    # Read records line by line, keeping continuation lines attached.
    def logical(name: str) -> list[str]:
        result: list[str] = []
        pending = ""
        for line in sections.get(name, []):
            pending += " " + line
            if pending.rstrip().endswith(","):
                continue
            result.append(pending.replace(",", " ").strip())
            pending = ""
        if pending:
            raise ValueError(f"Unterminated declaration in {name}")
        return result

    def unquote(value: str) -> str:
        value = value.strip()
        return value[1:-1].replace("''", "'") if value.startswith("'") and value.endswith("'") else value

    components = {}
    for record in logical("$PACKAGES"):
        left, right = record.split(";", 1)
        fields = left.split("!")
        if len(fields) != 3:
            raise ValueError(f"Invalid package: {record}")
        for ref in right.split():
            if ref in components:
                raise ValueError(f"Duplicate component {ref}")
            components[ref] = {"package": fields[0].strip(), "value": unquote(fields[2])}
    nets = {}
    pins = {}
    for record in logical("$NETS"):
        left, right = record.split(";", 1)
        net = unquote(left)
        if net in nets:
            raise ValueError(f"Duplicate net {net}")
        nets[net] = right.split()
        for pin in nets[net]:
            if pin in pins:
                raise ValueError(f"Pin {pin} appears in two nets")
            if pin.rsplit(".", 1)[0] not in components:
                raise ValueError(f"Unknown component in {pin}")
            pins[pin] = net
    if not components or not nets:
        raise ValueError("Missing package or net declarations")
    return {"components": components, "nets": nets, "pins": pins}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.input.read_bytes()
    data = parse(source.decode("utf-8-sig"))
    data["source"] = {"name": "Netlist_Schematic1_2026-11-23.tel", "library_file_id": "libfile_e53fe91f86388191bed26e414ac272f7", "normalized_sha256": hashlib.sha256(source).hexdigest(), "normalized_size": len(source)}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Parsed {len(data['components'])} components, {len(data['nets'])} nets, {len(data['pins'])} pins")


if __name__ == "__main__":
    main()
