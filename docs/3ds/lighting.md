## Info on the lighting implementation

### Missing shadow attenuation
Shadow attenuation samples a texture unit, and that likely needs render to texture for most games so that they can construct
their shadow map. As such the colors are not multiplied by the shadow attenuation value, so there's no shadows.

### Missing bump mapping
Bump mapping also samples a texture unit, most likely doesn't need render to texture however may need better texture sampling
implementation (such as GPUREG_TEXUNITi_BORDER_COLOR, GPUREG_TEXUNITi_BORDER_PARAM). Bump mapping would work for some things,
namely the 3ds-examples bump mapping demo, but would break others such as Toad Treasure Tracker with a naive `texture` implementation.

Also the CP configuration is missing, because it needs a tangent map implementation. It is currently marked with error_unimpl.

### samplerEnabledBitfields
Holds the enabled state of the lighting samples for various PICA configurations
As explained in https://www.3dbrew.org/wiki/GPU/Internal_Registers#GPUREG_LIGHTING_CONFIG0

```c
const bool samplerEnabled[9 * 7] = bool[9 * 7](
	// D0     D1     SP     FR     RB     RG     RR
	true,  false, true,  false, false, false, true,  // Configuration 0: D0, SP, RR
	false, false, true,  true,  false, false, true,  // Configuration 1: FR, SP, RR
	true,  true,  false, false, false, false, true,  // Configuration 2: D0, D1, RR
	true,  true,  false, true,  false, false, false, // Configuration 3: D0, D1, FR
	true,  true,  true,  false, true,  true,  true,  // Configuration 4: All except for FR
	true,  false, true,  true,  true,  true,  true,  // Configuration 5: All except for D1
	true,  true,  true,  true,  false, false, true,  // Configuration 6: All except for RB and RG
	false, false, false, false, false, false, false, // Configuration 7: Unused
	true,  true,  true,  true,  true,  true,  true   // Configuration 8: All
);
```

The above has been condensed to two uints for performance reasons.
You can confirm they are the same by running the following:
```c
const uint samplerEnabledBitfields[2] = { 0x7170e645u, 0x7f013fefu };
for (int i = 0; i < 9 * 7; i++) {
	unsigned arrayIndex = (i >> 5);
	bool b = (samplerEnabledBitfields[arrayIndex] & (1u << (i & 31))) != 0u;
	if (samplerEnabled[i] == b) {
		printf("%d: happy\n", i);
	} else {
		printf("%d: unhappy\n", i);
	}
}
```

### lightLutLookup
lut_id is one of these values
0 	D0
1 	D1
2 	SP
3 	FR
4 	RB
5 	RG
6 	RR 

lut_index on the other hand represents the actual index of the LUT in the texture
u_tex_luts has 24 LUTs for lighting and they are used like so:
0 		D0
1 		D1
2 		is missing because SP uses LUTs 8-15
3 		FR
4 		RB
5 		RG
6 		RR
8-15 	SP0-7
16-23 	DA0-7, but this is not handled in this function as the lookup is a bit different

The light environment configuration controls which LUTs are available for use
If a LUT is not available in the selected configuration, its value will always read a constant 1.0 regardless of the enable state in GPUREG_LIGHTING_CONFIG1
If RR is enabled but not RG or RB, the output of RR is used for the three components; Red, Green and Blue.

### Distance attenuation
Distance attenuation is computed differently from the other factors, for example
it doesn't store its scale in GPUREG_LIGHTING_LUTINPUT_SCALE and it doesn't use 
GPUREG_LIGHTING_LUTINPUT_SELECT. Instead, it uses the distance from the light to the
fragment and the distance attenuation scale and bias to calculate where in the LUT to look up.
See: https://www.3dbrew.org/wiki/GPU/Internal_Registers#GPUREG_LIGHTi_ATTENUATION_SCALE