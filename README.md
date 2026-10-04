# Frames, Signatures and Joints — Eternity II–type puzzle solvers

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.23132447.svg)](https://doi.org/10.5281/zenodo.23132447)

**Author:** Carlos Edison Guzman Marte (Calgary, Alberta, Canada; originally from the Dominican Republic)
**Period:** 29 September – 3 October 2026
**AI assistance:** the methods, ideas and decisions are the author's. The code, experiments and documentation were produced with the help of AI tools (Anthropic's Claude), under his direction. See [`paper/`](paper/) for the full statement.

This repository contains a short, intensive line of research on solvers for edge-matching puzzles of the Eternity II type. It has three solver families:

| Line | Folder | What it is |
|---|---|---|
| **Frames and signatures** (exact) | `src/v1_python` … `src/v4.7` | A backtracking solver organised around the frame of the board. It goes from a Python prototype to a multi-threaded C engine with side and row maps, Knuth-guided ordering, portfolios with restarts, and belief-propagation value ordering. |
| **Joints system** (stochastic + exact closure) | `src/v5_joints_system` | Keeps an always-assembled synthetic board (it chooses the colour of every joint) and introduces the real pieces by simulated annealing / parallel tempering. An exact "closure" finishes the last cells. |
| **GPU** | `src/v6_gpu` | Massive annealing, exhaustive search with a proof of no-solution, and a port of the v4.6 pruning to a GTX 1070 (CuPy/NVRTC). |

The full write-up is [`paper/eternity2_frames_signatures_joints.pdf`](paper/) (Markdown source alongside). It includes every result, the negative ones too.

## Highlights

**Exact solver, versions 4.0 → 4.6**

- An 11×11 board (16 colours) went from **206.8 s to 0.34 s** on 12 threads.
- Of 16 hard Harris-type 8×8 boards, the solver went from 9 solved in 5 minutes to 13–14.

**Belief propagation as value ordering (v4.7)**

- Five 9×9 9:3 boards with five clues went from **150 s to 33 s** in total.

**Interior-first ordering, chosen by Knuth's estimator (v4.3)**

- Interior-first ordering gave 15–32× on boards with few border colours.

**GPU exhaustive search**

- Speed: 1.3–1.9 billion nodes/s.
- Solved 10/10 Harris 8×8 7:2 boards in 0.4–29 s.

**Negative results**

- Colour parity, Hall matching, SAT (10–100× slower), recomputed or 2×2 beliefs, and heavy pruning on the GPU (10× slower than on the CPU) did not help.
- Stochastic search on 9×9 and larger falls into **false near-solutions**: in one case, 74/81 real pieces but only 7 in the right place.
- The real 16×16 puzzle remains far out of reach.

## Repository layout

```
paper/                 the article (PDF + Markdown source)
src/vX/                source, tests, logs and original Spanish docs (LEEME.md, RESULTADOS*.md) of every version
bin_windows/           prebuilt Windows binaries of the final versions (v4.7, v5, v6)
registry/              SHA-256 of every registered version package + OpenTimestamps proofs (.ots)
docs_es/               the author's original registration descriptions, notes and plans (Spanish)
CITATION.cff, .zenodo.json
```

The original documentation is in Spanish, the author's language. Each version folder has a `LEEME.md` (README) and a `RESULTADOS*.md` (results).

## Quick start

**Exact solver (v4.7).** It builds with any C compiler with pthreads (Linux/macOS/MinGW):

```
gcc -O3 -march=native -pthread -o e2marcos47 src/v4.7/e2marcos47.c -lm
./e2marcos47 BOARD.txt -t 12 -l 600                  # all threads, 600 s limit
./e2marcos47 BOARD.txt -t 12 -P 5,5,14,2 -P 3,3,35,1  # with fixed pieces (row,col,piece,clockwise turns)
```

- Optional belief ordering:
  `python src/v4.7/creencias.py BOARD.txt -P ... --salida beliefs.txt`, then add `--creencias beliefs.txt --valor 3`.
- GUIs: `ventana_v3.py` … `ventana_v46.py` (tkinter) in the v3–v4.6 folders. v4.7 uses the v4.6 window with the new options on the command line.

**Joints system (v5).** Run `python src/v5_joints_system/ventana_uniones.py` (panel). On Windows, run `Abrir_uniones.bat`.

**GPU (v6).** It needs an NVIDIA GPU, `cupy-cuda12x` and numpy. Run `python src/v6_gpu/ventana_gpu.py`, or the engines directly:

- `gpu_recocido.py` (annealing)
- `gpu_exacto.py` (exhaustive search)
- `gpu_dominios.py` (v4.6 pruning)

### Board format

The board format is plain text. A board is *n×n* pieces, 4 integers per piece in the order **N E S W**, with colour 0 for the grey border. Lines starting with `#` are comments.

Test boards and generators are included:

- `tableros/`
- `bateria_investigacion/`
- `tableros_conocidos/` (boards with a known solution)
- `tableros_tipo_oficial/` (official-type boards)

**The official Eternity II piece list and the official clue puzzles are not included.** They belong to their publisher. The solvers read any board in the format above.

## Timestamps

Every version was packed into a zip with a per-file SHA-256 manifest, registered, and timestamped on the Bitcoin blockchain with [OpenTimestamps](https://opentimestamps.org).

- The hashes are in `registry/SHA256SUMS_registration_zips.txt`.
- The proofs are in `registry/timestamps/`.
- The zip files themselves are kept by the author.

## License

- **Code:** MIT License (`LICENSE`).
- **Article and documentation:** Creative Commons Attribution 4.0 (CC BY 4.0) (`LICENSE-docs.md`).

Both require credit to the author, Carlos Edison Guzman Marte.

## Citation

See `CITATION.cff`. Suggested form:

> Guzman Marte, C. E. (2026). *Frames, Signatures and Joints: an experimental study of exact, stochastic and GPU solvers for Eternity II–type edge-matching puzzles.* Zenodo. https://doi.org/10.5281/zenodo.23132447
