# MAEG4060 — Off-road terrain sandbox (DirectX 9)

**Repository:** [github.com/Rex-Yim/Direct3DTerrainModels](https://github.com/Rex-Yim/Direct3DTerrainModels)

Course project: an interactive **DirectX 9.0c** sandbox on procedural mountainous terrain—vehicle dynamics, splatted terrain, props, and a windmill with hierarchy animation (`.x` / `ID3DXAnimationController` when present) or static-mesh drawing with code-driven motion when not.

## Screenshots

Crops match **Figure 1** and **Figure 2** in `docs/stageFinal.pdf` (same trimming as the LaTeX build).

**Figure 1 — Running build:** terrain splatting, windmill, props, and HUD.

![Figure 1: in-application sandbox](docs/images/readme_crop_figure1_sandbox.png)

**Figure 2 — CI:** GitHub Actions run for the *boulder and windmill models* workflow, with the application visible in the run summary.

![Figure 2: GitHub Actions run with sandbox inset](docs/images/readme_crop_figure2_ci.png)

**Cinema 4D viewports** (same sessions as `docs/media/c4d-*.mp4`; GIFs for inline README playback):

| Jeep (source mesh) | Windmill (source mesh) |
| --- | --- |
| ![C4D viewport: jeep](docs/images/c4d-car-viewport.gif) | ![C4D viewport: windmill](docs/images/c4d-windmill-viewport.gif) |

Full recordings: [`docs/media/c4d-car-viewport.mp4`](docs/media/c4d-car-viewport.mp4), [`docs/media/c4d-windmill-viewport.mp4`](docs/media/c4d-windmill-viewport.mp4).

**Modeling tool stills** (crate/boulder and terrain reference meshes—these appear as figures in the **3D models** section of `docs/stageFinal.pdf`):

| Props | Terrain reference |
| --- | --- |
| ![Crate and boulder in authoring tool](docs/images/modeling_viewport_crate_boulder.png) | ![Terrain mesh in authoring tool](docs/images/modeling_viewport_terrain.png) |

## Repository layout

- **Source:** `src/` — entry `main.cpp`, `Game.cpp`, terrain, vehicle, graphics, animation, input.
- **Build:** `CMakeLists.txt` — target `maeg4060_stage2` (Windows, MSVC, C++17, DirectX 9 / D3DX, optional XInput).
- **Assets:** `Assets/` — models, textures, terrain references.
- **Docs:** **`stageOne.pdf`**, **`stageTwo.pdf`**, and **`stageFinal.pdf`** (course stages), plus **`User_Guide.pdf`**, live in **`docs/`**. Replace **`stageFinal.pdf`** with your own export if needed. LaTeX sources are in **`docs/latex/`**. Rebuild with `make` or `docs/build-docs.bat` (see **`docs/README.md`**).

See **`docs/User_Guide.pdf`** (source: `docs/latex/user_guide.tex`) for controls, requirements, and how to run the executable.

## License / course

Academic coursework (CUHK MAEG4060). Authorship of the shipped meshes and how they are integrated in-engine are described in the report (section *3D models: authorship and engine integration*).
