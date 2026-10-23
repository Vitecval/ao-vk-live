# Weather and Halloween fog

Open AO-VK options with **Ctrl+Shift+F12**, use **Tab** to reach **Weather**, select a row with **Up/Down**, and change it with **Left/Right**. Changes apply and save immediately. **Backspace** resets the selected row.

- **Weather**: Automatic, Clear, Overcast, Rain or Thunderstorm. Strength controls selected rain and thunderstorms. Keep **New weather** enabled; enhanced clouds need **Clouds** enabled as well. Weather changes settle over a few seconds.
- **Fog**: Automatic, Halloween, Mist or Custom. Halloween uses a near-black horizon and sky, with scenery disappearing at about **30 metres** by default. Mist uses a pale grey-blue. Custom uses the red, green and blue controls.
- **Visibility distance**: how far you can see through selected fog. **Clear distance** keeps the air nearest the camera clear and is capped below the visibility distance.
- **Dim nearby scenery**: darkens the world close to you under Halloween or Custom fog. Mist leaves nearby lighting alone. This also takes account of the main Darkness setting.
- **Drifting patches**: adds gentle variation within the fog. Zero makes it still.
- **Apply indoors too**: off by default. Selected fog normally applies only outdoors.

Weather and fog are separate: combine Halloween fog with a clear sky or a thunderstorm, for example. These are local visual choices and do not change server weather, the clock or what other players see. Both selectors default to **Automatic**. Returning them to Automatic restores AO-VK's normal atmosphere without changing your saved grass or terrain settings. Switching **All enhancements** off also removes the overrides.

The Halloween mode is a recreation inspired by the [historical description and screenshots](https://wiki.aodb.us/index.php?title=Halloween), rather than a repair of Funcom's seasonal weather data. It works independently of whether the client currently requests seasonal fog.

| Setting | Values |
| --- | --- |
| `RVK_WeatherMode` | 0 Automatic, 1 Clear, 2 Overcast, 3 Rain, 4 Thunderstorm |
| `RVK_WeatherAmt` | 0.05–1, default 0.65 |
| `RVK_FogMode` | 0 Automatic, 1 Halloween, 2 Mist, 3 Custom |
| `RVK_FogRange` | 5–500 metres, default 30 |
| `RVK_FogClear` | 0–20 metres, default 3 |
| `RVK_FogDark` | 0–0.9, default 0.35 |
| `RVK_FogDrift` | 0–1, default 0.4 |
| `RVK_FogRed/Green/Blue` | Custom RGB, each 0–1 |
| `RVK_FogInside` | 0 outdoors only, 1 also indoors |

The legacy `RVK_RainForce` and `RVK_StormForce` remain in the ini/console and apply in Automatic mode. Use the new selector in the options panel.

## Implementation and verification

Selected fog is a depth-based pass at the end of world tone mapping, before held labels and interface composition. It uses radial camera distance and is applied after exposure and grading, so auto-brightening and bloom cannot restore a bright sky behind black fog. No native terrain replacement or game fog-enable flag is required. Transparent effects that do not write depth use the opaque surface behind them for their fog distance.

`weather_check` checks modes, bounds, colours and indoor opt-in without a GPU. The headless renderer supports `--fog-mode N`, `--fog-range M` and `--weather-mode N` for visual checks, for example `rvk_demo --shadow-test --hdr --ui-window --fog-mode 1 --frames 4 --shot halloween.bmp`. In-game appearance still needs comparison in representative outdoor zones.
