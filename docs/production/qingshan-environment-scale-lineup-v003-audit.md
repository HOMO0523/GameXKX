# Qingshan Scale Lineup v003 Bounding-Box Audit

Artifact: `SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_SCALE_LINEUP__board__v003.png`

This is an approximate concept-image audit, not a source of metric dimensions. The image is 1774 x 887 pixels. The shared baseline was detected at `y=674`. For each separated subject, the largest connected dark-foreground component inside its isolated horizontal band was measured from its top to the shared baseline. The hero's measured 95-pixel full silhouette is the comparison unit.

| Subject | Requested ratio | Measured pixels | Measured hero ratio |
|---|---:|---:|---:|
| Hero | 1.0 | 95 | 1.00 |
| S house | 3.8 | 123 | 1.29 |
| M house | 5.0 | 161 | 1.69 |
| L building | 6.0 | 187 | 1.97 |
| North gate | 12.0 | 345 | 3.63 |
| Bridge height | 5.0 | 105 | 1.11 |
| Dock height | 2.0 | 70 | 0.74 |
| Pine | 7.0 | 249 | 2.62 |
| Broadleaf | 6.0 | 218 | 2.29 |
| Bamboo | 6.0 | 215 | 2.26 |
| Shrub | 1.5 | 72 | 0.76 |
| Near mountain foot | 18.0 | 312 | 3.28 |

The bridge component is 215 pixels wide, or 2.26 measured hero heights. The north gate is 1.84 times the L building height, close to the requested two-to-one hierarchy. The near mountain is 0.90 times the gate height, so it is visually near the tallest tier but does not literally exceed the gate. Across all subjects, the largest-to-smallest measured height ratio is 4.93, demonstrating that v003 no longer equalizes the lineup into same-size icons.

Decision: v003 passes the non-equalization review gate and preserves the required single-row, low-detail presentation, but its numeric ratios remain generatively compressed. Production dimensions must continue to come from each asset JSON's `target_dimensions_m`, not from pixels in this concept board. This view has exhausted its 3/3 generation budget; v004 is forbidden.
