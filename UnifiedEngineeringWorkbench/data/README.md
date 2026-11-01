# Example data

All files in this folder are **SYNTHETIC**. They were generated for the assignment. They are not ANSYS exports, CAD vendor files, or laboratory measurements.

## example_geometry.stl

- Format: ASCII STL
- Units: metres
- Shape: a 1 m x 10 mm x 10 mm rectangular bar (visual aid only)
- Provenance: written by hand for this workbench
- Tested parser limits: 50,000 triangles, 40 MiB text

This geometry is **not registered** to the temperature CSV.

## example_temperature.csv

- Format: UTF-8 CSV
- Required columns: `node_id,x_m,y_m,z_m,temperature_C`
- Units: metres and degrees Celsius
- Temperatures follow the assignment analytical bar solution sampled on a coarse 3D lattice
- Labelled SYNTHETIC in the file header
- Tested parser limits: 100,000 nodes, 20 MiB text

Do not treat these nodes as the FEM mesh used in Task C.
