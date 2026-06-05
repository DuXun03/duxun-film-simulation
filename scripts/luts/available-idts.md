# FilmSim -- Available IDT Profiles

## LUT-based IDTs (in luts/ directory)

Use these .cube files as 3D LUTs in Resolve's CST node or LUT OFX.

- **ACEScct_to_Kodak2383** ¡ú `luts/ACEScct_to_Kodak2383.cube`
- **ACEScct_to_DWG** ¡ú `luts/ACEScct_to_DWG.cube`
- **BMD_URSAMini46k_Film_to_ACEScct** ¡ú `luts/BMD_URSAMini46k_Film_to_ACEScct.cube`
- **BMD_URSAMini4k_Film_to_ACEScct** ¡ú `luts/BMD_URSAMini4k_Film_to_ACEScct.cube`
- **BMD_Broadcast_Film_to_ACEScct** ¡ú `luts/BMD_Broadcast_Film_to_ACEScct.cube`
- **BMD_CinemaCamera_Film_to_ACEScct** ¡ú `luts/BMD_CinemaCamera_Film_to_ACEScct.cube`
- **BMD_Pocket6k_Film_to_ACEScct** ¡ú `luts/BMD_Pocket6k_Film_to_ACEScct.cube`
- **BMD_Pocket4k_Film_to_ACEScct** ¡ú `luts/BMD_Pocket4k_Film_to_ACEScct.cube`
- **ARRI_LogC4_to_Rec2020** ¡ú `luts/ARRI_LogC4_to_Rec2020.cube`
- **ARRI_LogC4_to_Rec709** ¡ú `luts/ARRI_LogC4_to_Rec709.cube`
- **OM_LOG400_BT2020_to_ACEScct** ¡ú `luts/OM_LOG400_BT2020_to_ACEScct.cube`
- **OM_LOG400_P3_to_ACEScct** ¡ú `luts/OM_LOG400_P3_to_ACEScct.cube`

## Math-based IDTs (built into mycolor engine)

These are implemented as GLSL/DCTL algorithms (no external LUT needed).
Available in FilmSim-Mycolor.h and can be exposed as DCTL modules in Phase 4.

| Profile | Color Space | Method |
|---------|-------------|--------|
| sRGB | sRGB ¡ú ACEScct | Inverse sRGB EOTF + gamut compression |
| Rec709 | Rec.709 ¡ú ACEScct | Inverse Rec.709 OETF + gamut compression |
| ArriLogC | ARRI LogC3 ¡ú ACEScct | Normalized logC to linear + AWG4 matrix |
| ArriLogC4 | ARRI LogC4 ¡ú ACEScct | LogC4 to scene-linear + AWG4 matrix (Alexa 35) |
| Cineon | Cineon Log ¡ú ACEScct | Cineon CV to density to linear + gamut compression |
| ProPhotoRGB | ProPhoto ¡ú ACEScct | Linear conversion + gamut compression |
| P3D60/D65 | Display P3 ¡ú ACEScct | EOTF inverse + gamut compression |
| VisionLog | VisionLog ¡ú ACEScct | Custom log curve + AWG4 matrix |
| BMD Gen5 | BMD Film Gen5 ¡ú ACEScct | Piecewise log curve + gamut compression |
| Bolex Log | BolexLog ¡ú ACEScct | Custom log curve + conversion matrix |
