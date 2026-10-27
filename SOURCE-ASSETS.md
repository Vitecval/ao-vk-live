# Source artwork

All master artwork, atlases, catalogs and provenance are retained. Deterministic
baked map outputs and hash-verified duplicate atlas crops are omitted to keep
this archive within GitHub release limits. No workspace originals were removed.

Install Pillow and NumPy, then run:

    python tools/restore-source-art.py
    python tools/bake-modern-icc.py
    python tools/bake-cyber-icc.py
    python tools/bake-character-art.py
    python tools/bake-world-art.py

The crop restoration checks every result against release/derived-art.json and
refuses to replace differing files. The Full HD download includes pre-baked maps.
