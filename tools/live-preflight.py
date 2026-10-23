#!/usr/bin/env python3
"""Read-only validation of the retail files audited for this experimental live build."""
import argparse
import hashlib
import pathlib
import sys

EXPECTED = {
    "randy31.dll": "9d9e0da25ac6f8c7ecdce7a47a2409575945966a29e3b77850f7c62033a04be3",
    "DisplaySystem.dll": "d4dd19f3277d70ff5fc0146b5474e8c5abdd55f45e81288fe018192669046c91",
    "Gamecode.dll": "654969a6b65946cb161f0e60aed8589260fc5eca1795488f66bb56f8fff73726",
    "GUI.dll": "e485384721e2fe13972e840dfb6a9fe29b1ba4eb71b42cc049e5097a570b6de1",
    "N3.dll": "8c019efd72d547879a06585b69147ab1546b9617a2fce090e5863791aec8b0bb",
    "FXS.dll": "381ad306d1889ebdcf11de3fc129d90c8802448df3465841ed5f3dc5206ed78a",
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("client", type=pathlib.Path)
    args = parser.parse_args()
    good = True
    for name, expected in EXPECTED.items():
        path = args.client / name
        actual = hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else "missing"
        match = actual == expected
        print(f"{'OK' if match else 'UNSUPPORTED'} {name}: {actual}")
        good &= match
    print("Classic live files verified." if good else "Files differ from the audited classic client. Do not install this build.")
    return 0 if good else 1

if __name__ == "__main__":
    sys.exit(main())
