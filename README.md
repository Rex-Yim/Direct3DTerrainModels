# MAEG4060 — Off-road terrain sandbox (DirectX 9)

Course project: an interactive **DirectX 9.0c** sandbox on procedural mountainous terrain—vehicle dynamics, splatted terrain, props, and a windmill with Maya-exported animation (`.X` / `ID3DXAnimationController`).

## Screenshots

**Running build** — terrain splatting, windmill, props, and HUD:

![Sandbox: terrain, windmill, props, and HUD](docs/images/screenshot_sandbox_windmill.png)

**CI** — GitHub Actions run for the *boulder and windmill models* workflow, with the application visible in the run summary:

![GitHub Actions run with sandbox inset](docs/images/screenshot_github_actions_run.png)

## Repository layout

- **Source:** `src/` — entry `main.cpp`, `Game.cpp`, terrain, vehicle, graphics, animation, input.
- **Build:** `CMakeLists.txt` — target `maeg4060_stage2` (Windows, MSVC, C++17, DirectX 9 / D3DX, optional XInput).
- **Assets:** `Assets/` — models, textures, terrain references.
- **Docs:** `docs/latex/` — final report and user guide sources; `docs/user_guide` material as referenced by the course submission.

See **`docs/latex/user_guide.tex`** (and packaged **User Guide PDF**) for controls, requirements, and how to run the executable.

## License / course

Academic coursework (HKU MAEG4060). Third-party 3D assets are credited in the report (**§3D models: sources and modifications**).
