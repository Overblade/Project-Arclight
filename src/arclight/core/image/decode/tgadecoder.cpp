/*
 *	 Copyright (c) 2022 - Arclight Team
 *
 *	 This file is part of Arclight. All rights reserved.
 *
 *	 tgadecoder.cpp
 */

#include "tgadecoder.hpp"
#include "stream/binaryreader.hpp"
#include "util/bool.hpp"
#include "util/unsupportedoperationexception.hpp"



struct TGAColorMapSpecification {

	u16 firstEntryIndex;
	u16 colorMapLength;
	u8 colorMapEntrySize;

};

struct TGAImageSpecification {
	
	u16 originX;
	u16 originY;
	u16 width;
	u16 height;
	u8 pixelDepth;
	u8 imageDescriptor;

};

struct TGAHeader {

	u8 idLength;
	u8 colorMapType;
	u8 imageType;
	TGAColorMapSpecification colorMapSpec;
	TGAImageSpecification imageSpec;

};



constexpr u32 getColorMapSize(const TGAColorMapSpecification& spec) {
	return spec.colorMapLength * spec.colorMapEntrySize / 8;
}

constexpr u8 getImageAlphaBits(const TGAImageSpecification& spec) {
	return Bits::mask(spec.imageDescriptor, 0, 4);
}

constexpr u8 getImageOriginMode(const TGAImageSpecification& spec) {
	return Bits::mask(spec.imageDescriptor, 4, 2);
}

constexpr u8 getImageReservedBits(const TGAImageSpecification& spec) {
	return Bits::mask(spec.imageDescriptor, 6, 2);
}

constexpr u32 getImageDataSize(const TGAImageSpecification& spec) {
	return spec.width * spec.height * spec.pixelDepth / 8;
}

constexpr u32 getTransformedX(const TGAImageSpecification& spec, u32 x) {
	return (getImageOriginMode(spec) & 0x10) ? spec.width - x - 1 : x;
}

constexpr u32 getTransformedY(const TGAImageSpecification& spec, u32 y) {
	return (getImageOriginMode(spec) & 0x20) ? y : spec.height - y - 1;
}



void TGADecoder::decode(std::span<const u8> data) {

	validDecode = false;

	BinaryReader reader(data);

	TGAHeader hdr{};

	// Read header
	hdr.idLength = reader.read<u8>();

	hdr.colorMapType = reader.read<u8>();

	if (hdr.colorMapType > 1)
		throw ImageDecoderException("Invalid color map type");

	hdr.imageType = reader.read<u8>();

	if (Bool::none(hdr.imageType, 0, 1, 2, 3, 9, 10, 11))
		throw ImageDecoderException("Invalid image type");

	// Read color map specification
	if (hdr.colorMapType) {

		hdr.colorMapSpec.firstEntryIndex = reader.read<u16>();
		hdr.colorMapSpec.colorMapLength = reader.read<u16>();
		hdr.colorMapSpec.colorMapEntrySize = reader.read<u8>();

	} else {
		reader.seek(5);
	}

	// Read image specification
	origin.x = hdr.imageSpec.originX = reader.read<u16>();
	origin.y = hdr.imageSpec.originY = reader.read<u16>();

	hdr.imageSpec.width = reader.read<u16>();

	if (hdr.imageSpec.width == 0)
		throw ImageDecoderException("Width should not be zero");

	hdr.imageSpec.height = reader.read<u16>();

	if (hdr.imageSpec.height == 0)
		throw ImageDecoderException("Width should not be zero");

	hdr.imageSpec.pixelDepth = reader.read<u8>();

	// TODO: handle supported pixel depths

	hdr.imageSpec.imageDescriptor = reader.read<u8>();

	if (getImageReservedBits(hdr.imageSpec))
		Log::warn("TGADecoder", "ImageDescriptor reserved bits are not zero");

	// Read image ID
	u8 idBuffer[255];

	if (hdr.idLength)
		reader.read<u8>({ idBuffer, hdr.idLength });

	// Read color map data
	if (hdr.colorMapType) {

		u32 colorMapSize = getColorMapSize(hdr.colorMapSpec);

		colorMapData.resize(colorMapSize);

		reader.read<u8>(colorMapData);

	}

	// TODO: implement RLE decompression
	
	// Read image data
	if (Bool::any(hdr.imageType, 1, 2, 3)) {

		u32 imageDataSize = getImageDataSize(hdr.imageSpec);

		imageData.resize(imageDataSize);

		reader.read<u8>(imageData);

	}

	switch (hdr.imageType) {

	case 0: // No image data
		image = Image<Pixel::BGRA8>(hdr.imageSpec.width, hdr.imageSpec.height).makeRaw();
		break;

	case 1: // Uncompressed, Color mapped
		parseColorMapImageData(hdr);
		break;

	case 2: // Uncompressed, True color
		parseTrueColorImageData(hdr);
		break;

	case 3: // Uncompressed, Black and white
		break;

	case 9: // Run-length encoded, Color mapped	
	case 10: // Run-length encoded, True color
	case 11: // Run-length encoded, Black and white

		throw UnsupportedOperationException("Run-Length Encoded images are not supported");

	}

	validDecode = true;

}



RawImage& TGADecoder::getImage() {

	if (!validDecode) {
		throw ImageDecoderException("Bad image decode");
	}

	return image;

}



void TGADecoder::parseColorMapImageData(const TGAHeader& hdr) {

	std::vector<PixelBGRA8> colorMap;

	auto convertColorMap = [this, hdr, &colorMap]<Pixel P, SizeT Size>() {

		auto colorMapView = std::span{ colorMapData };

		colorMap.resize(hdr.colorMapSpec.colorMapLength);
		
		for (u32 i = 0; i < hdr.colorMapSpec.colorMapLength; i++) {

			auto pixelData = colorMapView.subspan(i * Size, Size);

			colorMap[i] = PixelConverter::convert<Pixel::BGRA8>(PixelType<P>(pixelData));

		}

	};

	auto buildImage = [this, hdr, &colorMap]() {

		Image<Pixel::BGRA8> bufImage(hdr.imageSpec.width, hdr.imageSpec.height);

		for (u32 y = 0; y < hdr.imageSpec.height; y++) {

			// TODO: profile the usage of getter functions vs booleans for flipX/flipY

			u32 ry = getTransformedY(hdr.imageSpec, y);

			for (u32 x = 0; x < hdr.imageSpec.width; x++) {

				u32 rx = getTransformedX(hdr.imageSpec, x);

				SizeT imageIndex = x + y * hdr.imageSpec.width;

				u8 colorIndex = imageData[imageIndex];

				if (colorIndex >= hdr.colorMapSpec.colorMapLength)
					throw ImageDecoderException("Invalid color map index found in image data");

				bufImage.setPixel(rx, ry, colorMap[colorIndex]);

			}
		}

		image = bufImage.makeRaw();

	};

	// Convert color map to ARGB8
	switch (hdr.colorMapSpec.colorMapEntrySize) {

	case 16:
		convertColorMap.template operator()<Pixel::RGB5, 2>();
		break;

	case 24:
		convertColorMap.template operator()<Pixel::RGB8, 3>();
		break;

	case 32:
		convertColorMap.template operator()<Pixel::RGBA8, 4>();
		break;

	default:
		throw ImageDecoderException("Invalid color map color format");

	}

	switch (hdr.imageSpec.pixelDepth) {

	case 8:	
		buildImage();
		break;

	case 16:
	case 24:
	case 32:
		throw UnsupportedOperationException("Color map TGA format with %d-bits indices is not supported", hdr.imageSpec.pixelDepth);

	default:
		throw ImageDecoderException("Invalid pixel format");

	}

}



void TGADecoder::parseTrueColorImageData(const TGAHeader& hdr) {

	auto loadData = [this, hdr]<Pixel P, SizeT Size>() {

		BinaryReader reader(imageData);
		Image<Pixel::BGRA8> bufImage(hdr.imageSpec.width, hdr.imageSpec.height);

		for (u32 y = 0; y < hdr.imageSpec.height; y++) {

			// TODO: profile the usage of getter functions vs booleans for flipX/flipY

			u32 ry = getTransformedY(hdr.imageSpec, y);

			for (u32 x = 0; x < hdr.imageSpec.width; x++) {

				u32 rx = getTransformedX(hdr.imageSpec, x);

				u8 pixelData[Size];
				reader.read<u8>(pixelData);

				auto pixel = PixelConverter::convert<Pixel::BGRA8>(PixelType<P>(pixelData));

				bufImage.setPixel(rx, ry, pixel);

			}
		}

		image = bufImage.makeRaw();

	};

	switch (hdr.imageSpec.pixelDepth) {

	case 16:
		loadData.template operator()<Pixel::RGB5, 2>();
		break;

	case 24:
		loadData.template operator()<Pixel::RGB8, 3>();
		break;

	case 32:
		loadData.template operator()<Pixel::RGBA8, 4>();
		break;

	default:
		throw ImageDecoderException("Invalid pixel format");

	}

}
