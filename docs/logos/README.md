# Velk Brand Assets

## Colours

| Token | Hex | Usage |
|---|---|---|
| Mark standard | `#122438` | Box on light and dark-blue backgrounds |
| Mark contrast | `#3B6D7C` | Box on black and very dark backgrounds |
| Teal | `#18C4CE` | Dots — used in both versions |
| Text dark | `#0C0C0C` | Wordmark on light backgrounds |
| Text light | `#DDE8FF` | Wordmark on dark backgrounds |
| Text white | `#FFFFFF` | Wordmark on black / contrast-dark |

## Which version to use

| Background | Logo file |
|---|---|
| White / light | `velk-logo-standard-light` |
| Dark grey / dark blue | `velk-logo-standard-dark` |
| Black / OLED / terminal | `velk-logo-contrast-dark` |
| Light bg, contrast needed | `velk-logo-contrast-light` |

## Files

```
logo/
  velk-logo-standard-light.svg   SVG, use on light backgrounds
  velk-logo-standard-dark.svg    SVG, use on dark/dark-blue backgrounds
  velk-logo-contrast-light.svg   SVG, contrast version on light
  velk-logo-contrast-dark.svg    SVG, contrast version on black
  *@2x.png                       PNG at 520×160 (2×)

icon/
  velk-icon-standard.svg         Mark only, standard
  velk-icon-contrast.svg         Mark only, contrast
  velk-icon-{16,32,64,128,256,512}.png
  velk-icon-contrast-{16,32,64,128,256,512}.png

social/
  velk-github-social.png         1280×640 GitHub social preview
```

## Typeface

The wordmark uses **Consolas / Courier New** (monospace fallback stack).
For web use embed a monospace web font of your choice — the geometry is not
sensitive to the specific face as long as it is monospaced.
