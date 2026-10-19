# Desktop capacity measurements — 2026-10-09

D68 measurements supporting D71 presentation detail, D72 larger maps and D74 traffic scheduling. This is dated evidence; DEVELOPMENT_TASKS.md owns the working plan.

## Method

Windows Development package, D3D11, Ryzen 7 9800X3D / RTX 5080, native 3840x2160, scene scale 100%, secondary scale 100%, MSAA 4x, Day lighting, Clear weather and Stats HUD. Audio, VSync and frame cap are off. Runs are serial with no Editor/build or other game running. These offscreen timings do not establish laptop, SteamVR or Quest performance.

Recorded NVIDIA driver 32.0.16.1714; primary desktop mode 3840x2160 at 59Hz. The uncapped benchmark FPS is throughput, not monitor refresh. OS/build details are in each report.

The generated fixture uses the existing flat/dry generator (seed 7463), 883 connected road cells and real basic Home/Shop/Power/Water catalog buildings, residents, jobs, freight and citizen trips. Buildings face their roads and all placements/terrain access are validated. Funds are generous, zoning growth and disasters are disabled. Population stays constant in these runs. This is a controlled basic-lot workload, not a maximum-density/full-service city or natural-budget progression test. All cases remain 64x64 cells; this particular street/lot layout holds 744 buildings.

Warmup is 5 seconds after eight real simulation bootstrap steps. Ten capacity cases capture 75 wall-clock seconds, including monthly settlement; the separate 8x-MSAA render check captures 20 seconds and no month-end. Routing budget is citizen candidates per fixed step, independent of the visible-traffic cap and 2048 simulated-trip limit. Normal gameplay remains 32 candidates/64 visible, on every platform.

Frame intervals, capture duration and FPS use a monotonic wall clock, not actor DeltaSeconds, which Unreal clamps during long hitches. The runner verifies `frames * meanMs == captureSeconds * 1000`. Slow time is the percentage of elapsed frame time belonging to frames longer than 33.33ms. A high average FPS does not guarantee continuously smooth 30 FPS.

## Results

All cases use the same executable SHA256 `782D7BAC85D4AB58DA1A65587B2D4D6D1BC9694F0B52EB839F33F40ADEB8697E`.

| Buildings / residents | Speed | Route / visible budgets | Average FPS | p99 ms | Worst frame ms | Slow time % |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
|300 /1290|1x|32 /64|154.7|8.2|93.4|3.0|
|744 /3200|1x|32 /64|67.1|19.2|216.0|5.1|
|744 /3200|1x|32 /512|66.6|20.4|213.9|5.1|
|744 /3200|1x|512 /64|64.4|22.5|562.2|7.7|
|744 /3200|1x|512 /512|64.4|20.1|573.3|7.7|
|744 /3200|5x|32 /64|43.6|229.2|371.7|39.2|
|744 /3200, MSAA 8x*|1x|32 /64|64.9|24.2|214.5|7.1|
|500 /2150|5x|512 /512|47.1|396.2|803.7|54.2|
|650 /2790|5x|512 /512|33.1|572.4|903.6|58.6|
|700 /3000|5x|512 /512|33.0|613.3|895.5|58.6|
|744 /3200|5x|512 /512|27.7|700.9|989.0|60.7|

*The 20-second AA check is a rendering check, not a matched monthly/capacity comparison. Do not infer an exact 8x-MSAA cost or improvement from its average/percentiles. Simulation speed is the selected setting; start/end steps and months in each report describe the actual work, including any slowdown from engine time clamping during hitches.

## Findings

- At 1x with ordinary budgets, the tested 64x64 layout ceiling remains well above 30 FPS average. This is a measured lower bound for this workload; a larger-dimension maximum has not been measured.
- At 5x with 512 route candidates and 512 visible slots, 700 buildings/3000 residents is above 30 average and 744/3200 is below. This is an observed bracket, not an exact safe maximum. Long pauses already make this stress profile unsuitable as the normal preset, including at 500 buildings.
- Increasing only visible slots produces little change in this sample. With normal routing there are at most 139 traffic records; with expanded routing at 1x there are 334, and the heavy5x case reaches 777 (the renderer still caps at 512). These are combined traffic records, not a count of 512 distinct cars or GPU-visible draws. Richer mesh detail deserves a controlled comparison.
- Recurring CPU work dominates the dense basic case: derived refresh averages 12.46ms per call, including services 8.48ms, needs 2.91ms and road/traffic reconciliation 0.59ms. At 300 buildings these are 4.54/3.10/0.67/0.56ms. Current refresh runs on render-only/paused ticks too.
- At 744 buildings, ordinary fixed steps average 98.4ms and peak 189.2ms; 512 routing candidates raise these to 160.3/536.1ms at 1x. At 5x with expanded traffic they reach 266.6/894.4ms. Fixed-step time includes nested derived refresh, so those means must not be summed. Monthly figures measure the entire tick containing settlement, not settlement alone.
- Final process working sets span roughly 675–847MiB. Raw RHI primitives omit instancing/indirect geometry and are not triangle totals. Available RHI GPU frame cycles are whole-frame telemetry; CPU phase timings and controlled comparisons support the CPU finding.

## Reproduce and evidence

```powershell
& .\Tools\Run-DesktopBenchmark.ps1 -Lots 744 -Populated -Speed 1 -Weather Clear -Width 3840 -Height 2160 -DurationSeconds 75
& .\Tools\Run-DesktopBenchmark.ps1 -Lots 700 -Populated -Speed 5 -Weather Clear -Width 3840 -Height 2160 -CitizenRoutes 512 -VisibleTraffic 512 -DurationSeconds 75
```

`Artifacts/D68-Capacity-Wall-Summary.json` links all 11 reports and their per-run Metadata.json, arguments, build identity, clocks and population. Wrapper logs are `Artifacts/D68-Capacity-Wall-*.log`. Hardware/driver and final package identities are in `D68-Capacity-Hardware.json` and `D68-Capacity-Package-Identities.json`. Earlier non-Wall reports are exploratory, including clamped-clock/aborted runs, and are excluded from this table.

Next work belongs to D68/D74: reduce repeated services/needs and route/settlement bursts, preserving authority, emergency priority and saves. D72 must introduce versioned per-city dimensions and preserve 64x64 routing/terrain/save identities before a 96x96 trial. Cell-count scaling is not an FPS measurement. D71 can compare richer instanced assets with identical cities, caps and camera; Quest/PCVR retain separate device gates.
