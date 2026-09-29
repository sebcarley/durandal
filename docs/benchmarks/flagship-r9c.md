# Benchmark: flagship-r9c

- Date: 2026-09-26 11:10
- Commit: 261723c7 (durandal/round-9-sound)
- Machine: Mac17,3, Apple M5, 16 GB
- Power: AC Power
- Settings: uncapped (fps target 0), sound off, DURANDAL_QA=1 DURANDAL_SET=quality_tier=3

- Warm-up: one untimed run of L00-demo before measuring

## L00-demo-native

```
Durandal benchmark
  drawable: 1920 x 1080 px
  renderer: Metal display
  fps target: 0 (0 = uncapped), vsync off, 3 drawables
  frames logged: 33278 (summary excludes first 2 s)
  duration: 204.6 s, game ticks/s: 30.30
  average fps: 160.9
  worst second: 96 frames (at 183.0 s)
  1% low (p99 frame time): 101.9 fps (9.81 ms)
  longest frame: 16.55 ms
  frames between ticks (interpolated): 100.0%
  distinct world states shown: 32928 of 32928 frames
  cpu per frame (ms): wait 5.86, tick 0.00, render 0.33, present 0.01, other 0.01
  cpu in slow frames (>= p99, 330 frames, 34% on a tick): wait 10.58, tick 0.01, render 0.53, present 0.01, other 0.01
  dynamic lights: average 2.1, max 16
  presented: 160.4 fps, interval median 6.94 ms, p99 12.50 ms, refresh 4.17 ms (240 Hz), hitches (> 2x frame time) 351 of 32893 (1.07%)
```
Before the run: thermal state: fair; ; daemons: runningboardd 16.0%; 

## L06-combat-native

```
Durandal benchmark
  drawable: 1920 x 1080 px
  renderer: Metal display
  fps target: 0 (0 = uncapped), vsync off, 3 drawables
  frames logged: 29978 (summary excludes first 2 s)
  duration: 204.3 s, game ticks/s: 30.29
  average fps: 145.0
  worst second: 101 frames (at 49.1 s)
  1% low (p99 frame time): 103.2 fps (9.69 ms)
  longest frame: 100.89 ms
  frames between ticks (interpolated): 100.0%
  distinct world states shown: 29623 of 29623 frames
  cpu per frame (ms): wait 6.55, tick 0.01, render 0.32, present 0.01, other 0.01
  cpu in slow frames (>= p99, 297 frames, 33% on a tick): wait 9.98, tick 0.32, render 0.47, present 0.02, other -0.01
  dynamic lights: average 4.0, max 16
  presented: 144.7 fps, interval median 8.33 ms, p99 12.50 ms, refresh 4.17 ms (240 Hz), hitches (> 2x frame time) 5 of 29623 (0.02%)
```
Before the run: thermal state: fair; ; daemons: BiomeAgent 11.6%; 

## net-5D-space-native

```
Durandal benchmark
  drawable: 1920 x 1080 px
  renderer: Metal display
  fps target: 0 (0 = uncapped), vsync off, 3 drawables
  frames logged: 47512 (summary excludes first 2 s)
  duration: 295.0 s, game ticks/s: 30.30
  average fps: 159.8
  worst second: 116 frames (at 221.4 s)
  1% low (p99 frame time): 117.2 fps (8.53 ms)
  longest frame: 11.51 ms
  frames between ticks (interpolated): 100.0%
  distinct world states shown: 47139 of 47139 frames
  cpu per frame (ms): wait 5.82, tick 0.00, render 0.42, present 0.01, other 0.01
  cpu in slow frames (>= p99, 472 frames, 29% on a tick): wait 8.88, tick 0.01, render 0.45, present 0.01, other 0.01
  dynamic lights: average 0.9, max 16
  presented: 159.4 fps, interval median 6.94 ms, p99 9.63 ms, refresh 4.17 ms (240 Hz), hitches (> 2x frame time) 127 of 47109 (0.27%)
```
Before the run: thermal state: fair; ; daemons: Claude 10.8%; 

## own-260926-1-native

```
Durandal benchmark
  drawable: 1920 x 1080 px
  renderer: Metal display
  fps target: 0 (0 = uncapped), vsync off, 3 drawables
  frames logged: 15913 (summary excludes first 2 s)
  duration: 99.3 s, game ticks/s: 30.30
  average fps: 156.9
  worst second: 128 frames (at 24.5 s)
  1% low (p99 frame time): 128.8 fps (7.77 ms)
  longest frame: 19.27 ms
  frames between ticks (interpolated): 100.0%
  distinct world states shown: 15579 of 15579 frames
  cpu per frame (ms): wait 6.06, tick 0.00, render 0.29, present 0.01, other 0.01
  cpu in slow frames (>= p99, 156 frames, 33% on a tick): wait 8.12, tick 0.01, render 0.38, present 0.01, other 0.03
  dynamic lights: average 0.8, max 10
  presented: 156.0 fps, interval median 8.33 ms, p99 8.99 ms, refresh 4.17 ms (240 Hz), hitches (> 2x frame time) 1 of 15572 (0.01%)
```
Before the run: thermal state: fair; ; daemons: BiomeAgent 12.1%; 

## own-260926-2-native

```
Durandal benchmark
  drawable: 1920 x 1080 px
  renderer: Metal display
  fps target: 0 (0 = uncapped), vsync off, 3 drawables
  frames logged: 18477 (summary excludes first 2 s)
  duration: 125.0 s, game ticks/s: 30.30
  average fps: 144.8
  worst second: 90 frames (at 29.8 s)
  1% low (p99 frame time): 92.9 fps (10.76 ms)
  longest frame: 20.89 ms
  frames between ticks (interpolated): 100.0%
  distinct world states shown: 18098 of 18098 frames
  cpu per frame (ms): wait 6.56, tick 0.01, render 0.32, present 0.01, other 0.01
  cpu in slow frames (>= p99, 181 frames, 38% on a tick): wait 12.55, tick 0.01, render 0.43, present 0.01, other 0.01
  dynamic lights: average 0.9, max 16
  presented: 144.2 fps, interval median 8.33 ms, p99 12.50 ms, refresh 4.17 ms (240 Hz), hitches (> 2x frame time) 43 of 18094 (0.24%)
```
Before the run: thermal state: fair; ; daemons: BiomeAgent 12.2%; 

## own-260926-3-native

```
Durandal benchmark
  drawable: 1920 x 1080 px
  renderer: Metal display
  fps target: 0 (0 = uncapped), vsync off, 3 drawables
  frames logged: 8130 (summary excludes first 2 s)
  duration: 57.4 s, game ticks/s: 30.29
  average fps: 136.2
  worst second: 88 frames (at 21.7 s)
  1% low (p99 frame time): 96.6 fps (10.35 ms)
  longest frame: 12.68 ms
  frames between ticks (interpolated): 100.0%
  distinct world states shown: 7814 of 7814 frames
  cpu per frame (ms): wait 6.95, tick 0.01, render 0.37, present 0.01, other 0.01
  cpu in slow frames (>= p99, 79 frames, 35% on a tick): wait 11.00, tick 0.01, render 0.61, present 0.01, other 0.01
  dynamic lights: average 0.3, max 4
  presented: 136.1 fps, interval median 8.33 ms, p99 12.50 ms, refresh 4.17 ms (240 Hz), hitches (> 2x frame time) 1 of 7812 (0.01%)
```
Before the run: thermal state: fair; ; daemons: BiomeAgent 12.7%; 

