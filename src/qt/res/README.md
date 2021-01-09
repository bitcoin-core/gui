# Icon policy
This document provides guidelines for contributing icons to Bitcoin Core.

## Preparing Icons
Both an icon source file, in Scalable Vector Graphics (SVG) format,
and an optimized production file, in Portable Network Graphics (PNG) format,
are required for each icon.

#### SVG Source File
SVGs are used as source files because they can scale while retaining image quality.
They are not used in production due to limited application support.
If different-sized production (PNG) icons are required,
they can be generated from the associated SVG source file in any vector-based tool.

#### PNG Production File
PNGs are used in production due to wide application support, transparency support,
and better image quality compared to competing file types such as JPEG.

### Optimizing Production Files (PNG):
Production (PNG) files must be processed by the [optimize-png.py](https://github.com/bitcoin-core/bitcoin-maintainer-tools/blob/master/optimise-pngs.py) script before their inclusion in Bitcoin Core.
PNG optimization removes various unnecessary color profiles, ancillary (alla),
and text chunks, resulting in a lossless reduction of the file's size.

## Contributing
Bitcoin Core primarily uses icons from the [Bitcoin Icon set](https://github.com/BitcoinDesign/Bitcoin-Icons),
an open source icon set made for Bitcoin applications.
If a proposed feature requires an icon to be designed,
an issue should be opened in the [Bitcoin Icons repo](https://github.com/BitcoinDesign/Bitcoin-Icons/issues)
with the request.

Icons are not to be added to Bitcoin Core prior to a production use case.
If an icon is not being used, it should be removed.

SVGs are to be included under the `src/qt/res/src` directory.
Optimized PNGs are to be included under the `src/qt/res/icons` directory.

## Attribution
Icon additions must include appropriate attribution to the author, license information, and any comments relevant to the icon documented under the
[contrib/debian/copyright](https://github.com/bitcoin-core/gui/blob/master/contrib/debian/copyright) file.
