Workflow for Diagrams
=====================

1. Create diagram in draw.io (or other vector based program)
2. export vector based file: .svg is important, .xml is native to draw.io
3. export a .png with background set to transparent
4. find background image(s)
5. import into image editing softare (I use gimp, but anything will work)
6. create final arrangement with layers
    a all layers should be set to semi-transparent except top diagram
    b the diagram should be EASILY visible on the background. 
7. export as native file (e.g. xcf for gimp) preserving all layer information
8. export as .png without any transparency (e.g. white background layer at 100% opaque)
    a I use naming convention <subsystem>-complete.png e.g. Audio-complete.png
