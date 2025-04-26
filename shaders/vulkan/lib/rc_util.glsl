/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

ivec3 XYRtoUVW(ivec3 xyr, int factor, int cascadeNum) {
	ivec2 samplePos = ivec2(xyr.xy >> cascadeNum) << cascadeNum;
	int remainder = xyr.z % factor;
	samplePos += ivec2(remainder / 2, remainder % 2);
	return ivec3(samplePos, xyr.z / factor);
}

ivec3 UVWtoXYR(ivec3 uvw, int factor, int cascadeNum) {
	ivec2 samplePos = ivec2(uvw.xy >> cascadeNum) << cascadeNum;
	int remainder = (uvw.x - samplePos.x) * 2 + (uvw.y - samplePos.y);
	samplePos += (1 << cascadeNum) / 2;
	int r = uvw.z * factor + remainder;
	return ivec3(samplePos, r);
}
