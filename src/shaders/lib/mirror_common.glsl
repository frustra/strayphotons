#ifndef MIRROR_COMMON_INCLUDED
#define MIRROR_COMMON_INCLUDED

#define MIRROR_SOURCE_BIT 0x80000000

uint PackNoSourceMirror(int mirror) {
	return uint(mirror);
}

uint PackLightAndMirror(int light, int mirror) {
	return (uint(light) << 16) + uint(mirror);
}

uint PackMirrorAndMirror(int mirrorIndex, int mirrorId) {
	return PackLightAndMirror(mirrorIndex, mirrorId) | MIRROR_SOURCE_BIT;
}

int UnpackMirrorSource(uint tuple) {
	return int((tuple >> 16) & 0x7FFF);
}

int UnpackMirrorDest(uint tuple) {
	return int(tuple & 0x7FFF);
}

bool MirrorSourceIsMirror(uint tuple) {
	return (tuple & MIRROR_SOURCE_BIT) == MIRROR_SOURCE_BIT;
}

#endif