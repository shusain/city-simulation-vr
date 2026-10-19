# Newspaper typefaces

Unmodified DejaVu Serif regular and bold faces, obtained from the pinned Matplotlib v3.10.0 font bundle:

- https://raw.githubusercontent.com/matplotlib/matplotlib/v3.10.0/lib/matplotlib/mpl-data/fonts/ttf/DejaVuSerif.ttf
- https://raw.githubusercontent.com/matplotlib/matplotlib/v3.10.0/lib/matplotlib/mpl-data/fonts/ttf/DejaVuSerif-Bold.ttf
- https://raw.githubusercontent.com/matplotlib/matplotlib/v3.10.0/lib/matplotlib/mpl-data/fonts/ttf/LICENSE_DEJAVU

The accompanying `LICENSE_DEJAVU` preserves the Bitstream/Arev notices and permissions. Its identical companion under `CitySimVR/Content/CitySim/UI/Fonts/Notices` is staged with game packages. Glyph data is unchanged.

After an Editor compile, run the existing CityDesktopUiAssets commandlet with `-Newspaper`. It creates the two inline FontFace assets, the regular/bold runtime composite font and the newspaper widget/navigation in the existing Desktop HUD. Later calls reuse the font and update only this composition; no full UI rebuild is needed. Font source and cooked asset hashes are recorded with D90/Newspaper delivery evidence.
