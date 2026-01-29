# Build-time special effects

Line tags of value $900 \leq x \leq 999$ are considered "precious" and will not trigger a segment split, unless all other options are exhausted.
This is used to protect deep water and invisible lifts/stairs from being messed up accidentally by splits

| Line Tag | Supported in ELFBSP? | Description |
|----------|----------------------|-------------|
| 998      | :heavy_check_mark:   | Do not render segments for this linedef. |
| 999      | :heavy_check_mark:   | Do not add this linedef to the blockmap. |

| Line Special | Supported in ELFBSP? | Description |
|--------------|----------------------|-------------|
|   48         | :x:                  | The tag number will determine left scrolling speed by creating additional dummy linedefs as needed. |
|   85         | :x:                  | The tag number will determine right scrolling speed by creating additional dummy linedefs as needed. |
| 1048         | :x:                  | Remote, tagged left scroll. Speed also determined by tag value (modulo 100) creating dummy linedefs. |
| 1049         | :x:                  | Remote, tagged right scroll. Speed also determined by tag value (modulo 100) creating dummy linedefs. |
| 1078         | :x:                  | Change start vertex of all tagged lines to be the same as this line. Makes line non-render, and copies sidedefs. |
| 1079         | :x:                  | Change end vertex of all tagged lines to be the same as this line. Makes line non-render, and copies sidedefs. |
| 1080         | :heavy_check_mark:   | Increment segment angle by degrees defined in line tag. |
| 1081         | :heavy_check_mark:   | Set segment angle to degrees defined in line tag. |
| 1082         | :heavy_check_mark:   | Increment segment angle to BAM defined in line tag. |
| 1083         | :heavy_check_mark:   | Set segment angle to BAM defined in line tag. |
| 1084         | :warning:            | Do not render segment for this linedef on the front/right side. |
| 1085         | :warning:            | Do not render segment for this linedef on the back/left side. |
| 1086         | :warning:            | Do not render segment for this linedef on either sides. |
| 1087         | :x:                  | Do not split segment for this line. |
| 1088         | :x:                  | Change associated segment's target linedef index to current linedef's tag. |
