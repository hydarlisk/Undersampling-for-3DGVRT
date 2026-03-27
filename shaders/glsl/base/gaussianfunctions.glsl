/*
 * Abura Soba, 2025
 *
 * gaussianfunctions.glsl
 *
 */

vec2 intersectAABB(const Aabb aabb, vec3 rayOri, vec3 rayDir) {
    vec3 rayDirRcp = 1 / rayDir;
    vec3 t0 = (vec3(aabb.minX, aabb.minY, aabb.minZ) - rayOri) * rayDirRcp;
    vec3 t1 = (vec3(aabb.maxX, aabb.maxY, aabb.maxZ) - rayOri) * rayDirRcp;
    vec3 tmax = vec3(max(t0.x, t1.x), max(t0.y, t1.y), max(t0.z, t1.z));
    vec3 tmin = vec3(min(t0.x, t1.x), min(t0.y, t1.y), min(t0.z, t1.z));
    float maxOfMin = max(0.0f, max(tmin.x, max(tmin.y, tmin.z)));
    float minOfMax = min(tmax.x, min(tmax.y, tmax.z));
    return vec2(maxOfMin, minOfMax);
}

float particleResponse(float grayDist) {
    switch (PARTICLE_KERNEL_DEGREE) {
    case 8: // Zenzizenzizenzic
    {
        float s = -0.000685871056241f;
        const float grayDistSq = grayDist * grayDist;
        return exp(s * grayDistSq * grayDistSq);
    }
    case 5: // Quintic
    {
        float s = -0.0185185185185f;
        return exp(s * grayDist * grayDist * sqrt(grayDist));
    }
    case 4: // Tesseractic
    {
        float s = -0.0555555555556f;
        return exp(s * grayDist * grayDist);
    }
    case 3: // Cubic
    {
        float s = -0.166666666667f;
        return exp(s * grayDist * sqrt(grayDist));
    }
    case 1: // Laplacian
    {
        float s = -1.5f;
        return exp(s * sqrt(grayDist));
    }
    case 0: // Linear
    {
        /* static const */ float s = -0.329630334487f;
        return max(1.f + s * sqrt(grayDist), 0.f);
    }
    default: // Quadratic
    {
        float s = -0.5f;
        return exp(s * grayDist);
    }
    }
}


void fetchParticleDensity(
    const uint particleIdx,
    out vec3 particlePosition,
    out vec3 particleScale,
    out mat3 particleRotation,
    out float particleDensity) {
    //const ParticleDensity particleData = particleDensities.d[nonuniformEXT(particleIdx)];
    const ParticleDensity particleData = particleDensities.d[particleIdx];

    particlePosition = particleData.position;
    particleScale = particleData.scale;
    particleRotation = quaternionWXYZToMatrix(particleData.quaternion);
    particleDensity = particleData.density;
}

// load spherical harmonics coefficient
void fetchParticleSphCoefficients(
    const uint particleIdx,
    out vec3 sphCoefficients[SPH_MAX_NUM_COEFFS]) {
    const uint particleOffset = particleIdx * SPH_MAX_NUM_COEFFS * 3;	// each has 3 elements
    for (uint i = 0; i < SPH_MAX_NUM_COEFFS; i++) {
        uint offset = i * 3;	// each has 3 elements
        sphCoefficients[i] = vec3(
            //particleSphCoefficients.c[nonuniformEXT(particleOffset + offset + 0)],
            //particleSphCoefficients.c[nonuniformEXT(particleOffset + offset + 1)],
            //particleSphCoefficients.c[nonuniformEXT(particleOffset + offset + 2)]);
            particleSphCoefficients.c[particleOffset + offset + 0],
            particleSphCoefficients.c[particleOffset + offset + 1],
            particleSphCoefficients.c[particleOffset + offset + 2]);
    }
}

// calc spherical harmonics with coefficients
// not a special algorithm
// algorithm from spherical harmonics
// "parametric radiance function" in paper
vec3 radianceFromSpH(uint deg, const vec3 sphCoefficients[SPH_MAX_NUM_COEFFS], const vec3 rdir, bool clamped) {
    vec3 rad = SH_C0 * sphCoefficients[0];
    if (deg > 0) {
        const vec3 dir = rdir;

        const float x = dir.x;
        const float y = dir.y;
        const float z = dir.z;
        rad = rad - SH_C1 * y * sphCoefficients[1] + SH_C1 * z * sphCoefficients[2] - SH_C1 * x * sphCoefficients[3];

        if (deg > 1) {
            const float xx = x * x, yy = y * y, zz = z * z;
            const float xy = x * y, yz = y * z, xz = x * z;
            rad = rad + SH_C2[0] * xy * sphCoefficients[4] + SH_C2[1] * yz * sphCoefficients[5] +
                SH_C2[2] * (2.0f * zz - xx - yy) * sphCoefficients[6] +
                SH_C2[3] * xz * sphCoefficients[7] + SH_C2[4] * (xx - yy) * sphCoefficients[8];
            if (deg > 2) {
                rad = rad + SH_C3[0] * y * (3.0f * xx - yy) * sphCoefficients[9] +
                    SH_C3[1] * xy * z * sphCoefficients[10] +
                    SH_C3[2] * y * (4.0f * zz - xx - yy) * sphCoefficients[11] +
                    SH_C3[3] * z * (2.0f * zz - 3.0f * xx - 3.0f * yy) * sphCoefficients[12] +
                    SH_C3[4] * x * (4.0f * zz - xx - yy) * sphCoefficients[13] +
                    SH_C3[5] * z * (xx - yy) * sphCoefficients[14] +
                    SH_C3[6] * x * (xx - 3.0f * yy) * sphCoefficients[15];
            }
        }
    }
    rad += 0.5f;
    return clamped ? max(rad, vec3(0.0f)) : rad;
}

vec3 radianceFromSpH_Direct(uint deg, uint gaussianID, vec3 rdir) {
    uint base = gaussianID * SPH_MAX_NUM_COEFFS * 3;

#define GET_SH_COEFF(gBase, cidx) \
    vec3(particleSphCoefficients.c[(gBase) + (cidx) * 3],   \
        particleSphCoefficients.c[(gBase) + (cidx) * 3 + 1],   \
        particleSphCoefficients.c[(gBase) + (cidx) * 3 + 2])

    vec3 rad = SH_C0 * GET_SH_COEFF(base, 0);



    if (deg > 0) {
        const float x = rdir.x;
        const float y = rdir.y;
        const float z = rdir.z;

        // 1도 성분
        rad = rad - SH_C1 * y * GET_SH_COEFF(base, 1);
            + SH_C1 * z * GET_SH_COEFF(base, 2)
            - SH_C1 * x * GET_SH_COEFF(base, 3);

        if (deg > 1) {
            const float xx = x * x, yy = y * y, zz = z * z;
            const float xy = x * y, yz = y * z, xz = x * z;
            
            // 2도 성분
            rad = rad + SH_C2[0] * xy * GET_SH_COEFF(base, 4)
                + SH_C2[1] * yz * GET_SH_COEFF(base, 5)
                + SH_C2[2] * (2.0f * zz - xx - yy) * GET_SH_COEFF(base, 6)
                + SH_C2[3] * xz * GET_SH_COEFF(base, 7)
                + SH_C2[4] * (xx - yy) * GET_SH_COEFF(base, 8);

            if (deg > 2) {
                // 3도 성분
                rad = rad + SH_C3[0] * y * (3.0f * xx - yy) * GET_SH_COEFF(base, 9)
                    + SH_C3[1] * xy * z * GET_SH_COEFF(base, 10)
                    + SH_C3[2] * y * (4.0f * zz - xx - yy) * GET_SH_COEFF(base, 11)
                    + SH_C3[3] * z * (2.0f * zz - 3.0f * xx - 3.0f * yy) * GET_SH_COEFF(base, 12)
                    + SH_C3[4] * x * (4.0f * zz - xx - yy) * GET_SH_COEFF(base, 13)
                    + SH_C3[5] * z * (xx - yy) * GET_SH_COEFF(base, 14)
                    + SH_C3[6] * x * (xx - 3.0f * yy) * GET_SH_COEFF(base, 15);
            }
        }
    }

    rad += 0.5f;
    return max(rad, vec3(0.0f)); // clamped=true 고정 처리
}

bool processHit(
	vec3 rayOrigin,
	vec3 rayDirection,
	uint particleIdx,
	float minParticleKernelDensity,
	float minParticleAlpha,
	uint sphEvalDegree,
	inout float transmittance,
	inout vec3 radiance,
	inout float depth
#if SIMILARITY_VAR
    ,out float alphaOut
    ,out float weightOut
#endif
	){
	vec3 particlePosition;
	vec3 particleScale;
	mat3 particleRotation;
	float particleDensity;
	
	fetchParticleDensity(
        particleIdx,
        particlePosition,
        particleScale,
        particleRotation,
        particleDensity);

	//const vec3 giscl   = vec3(1 / particleScale.x, 1 / particleScale.y, 1 / particleScale.z);
	const vec3 giscl   = 1 / particleScale;
    const vec3 gposc   = (rayOrigin - particlePosition);
    const vec3 gposcr  = (particleRotation * gposc);
    const vec3 gro     = giscl * gposcr;
    const vec3 rayDirR = particleRotation * rayDirection;
    const vec3 grdu    = giscl * rayDirR;
    const vec3 grd     = safeNormalize(grdu);

	const vec3 gcrod = cross(grd, gro);
	const float grayDist = dot(gcrod, gcrod);

	const float gres = particleResponse(grayDist);
	const float galpha = min(0.99f, gres * particleDensity);

	//const bool acceptHit = (gres > minParticleKernelDensity) && (galpha > minParticleAlpha);
	bool acceptHit = (gres > minParticleKernelDensity) && (galpha > minParticleAlpha);
    //acceptHit = true;
	//bool acceptHit = (gres > minParticleKernelDensity) && (galpha > minParticleAlpha);
	if (acceptHit) {
        const float weight = galpha * (transmittance);
#if SIMILARITY_VAR
        alphaOut = galpha;
        weightOut = weight;
#endif
		const vec3 grds = particleScale * grd * dot(grd, -1 * gro);

		/*vec3 sphCoefficients[SPH_MAX_NUM_COEFFS];
		fetchParticleSphCoefficients(
			particleIdx,
			sphCoefficients);
		const vec3 grad = radianceFromSpH(sphEvalDegree, sphCoefficients, rayDirection, true);*/
        const vec3 grad = radianceFromSpH_Direct(sphEvalDegree, particleIdx, rayDirection);

		radiance += grad * weight;
		transmittance *= (1 - galpha);
	}

	return acceptHit;
}