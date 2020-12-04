# Icon policy
This document provides guidelines for contributing icons to Bitcoin Core.



## Preparing Icons
Both an icon source file, in Scalable Vector Graphics (SVG) format, and an optimized production file, in Portable Network Graphics (PNG) format, are required for 
each icon.

#### SVG Source File
SVGs are used as source files because they can scale while retaining image quality, but are not used in production due to limited application support.
In the event that different-sized production (PNG) icons are required, they can be generated from the associated SVG source file in a vector-based tool such as:
Inkspace, Adobe Illustrator, Figma, Sketch, or Adobe XD.

#### PNG Production File
PNGs are used in production due to wide application support, transparency support, and better image quality in comparison to competing file types
such as JPEG.


### Icon Grid
Bitcoin Core uses an 8-point grid system for its icons which utilizes size increments of 8px to visually orientate elements within a grid. The majority of screen sizes are divisible by 8 which makes fitting elements easier using an 8-point grid system. This makes things more efficient for designers and maintains consistency across applications. 


### Icon Style
Icon style should adhere to the following principles to maintain consistency and clarity:

- To ensure icons look good at any scale, a minimalistic design should be used.  
- Icon size should be 32x32px by default. A different sized icon may be necessary under special circumstances. In the event that the 32x32px size is too small for the required icon placement, then the icon should be scaled up or down while adhering to the 8-point grid system.
- Icon should avoid organic shapes or elements, these do not scale well.
- Colors should be consistent with existing Bitcoin Core icons. 
- Icons should adhere to the 8-point grid system (see above). 


### Optimizing Production Files (PNG): 
Production (PNG) files are to be optmized before being added to Bitcoin Core. Optimizing PNG files removes various color profiles, ancillary (alla) and text chunks resulting in a reduction in size of the image file losslessly (without a lowered image quality). Any `zopflipng` / `pngcrush` based PNG optimizers can be used, below are some examples:

- Open source tool [ImageOptim](https://imageoptim.com/api).
- Open source tool [Trimage](https://trimage.org/).
- (Advanced) Run them through Bitcoin Cores [optimize-png.py](https://github.com/bitcoin-core/bitcoin-maintainer-tools/blob/master/optimise-pngs.py) script.



## Contributing
Icons should only be added to the Bitcoin Core repo via a pull request (PR) in two situations:

- An icon used in production is being replaced with a new icon.
- A feature is being added to the Bitcoin Core GUI that requires the addition of a new icon.

If a new icon is designed for an upcoming or active PR, the designer of the icon should [open an issue](https://github.com/bitcoin-core/gui/issues/new/choose) to get feedback on the design and to make sure it is consistent with Bitcoin Core's iconography.

If a new feature is being added to the Bitcoin Core GUI that requires a new icon the developer should [open an issue](https://github.com/bitcoin-core/gui/issues/new/choose) requesting an icon to be designed. 

Icons, both SVG and PNG, are not to be added to Bitcoin Core prior to a production use case.

When opening a PR that intends to add an icon, a SVG source file must be included under `src/qt/res/src`, and a optimized production file (PNG) must be included under `src/qt/res/icons`. Both source and production files should be 32x32px in size.



## Attribution 
Icon additions must include appropriate attribution to the author, license information, and any comments relevant to the icon documented under the
[contrib/debian/copyright](https://github.com/bitcoin-core/gui/blob/master/contrib/debian/copyright) file.
