/**
 * @file LALSQTPNWaveform.c
 *		Contains the function definition to create GWforms.
 * @author László Veréb
 * @date 2010.05.21.
 */

#include <lal/LALSQTPNWaveform.h>
#include <lal/LALSQTPNIntegrator.h>
#include <lal/LALSQTPNWaveformInterface.h>

NRCSID (LALSQTPNWAVEFORMC, "$Id LALSQTPN_Waveform.c$");

/**		The macro function calculates the scalar product of two vectors.
 * @param[in]  a1	: the left vector
 * @param[in]  a2	: the right vector
 * @return	the product
 */
#define SCALAR_PRODUCT3(a1, a2) \
	((a1)[0] * (a2)[0] + (a1)[1] * (a2)[1] + (a1)[2] * (a2)[2]);

/**		The macro function calculates the vector product of two vectors.
 * @param[in]  left		: the left vector
 * @param[in]  right	: the right vector
 * @param[out] product	: the vector product
 */
#define VECTOR_PRODUCT3(left, right, product)\
	(product)[0] = ((left)[1] * (right)[2] - (left)[2] * (right)[1]);\
	(product)[1] = ((left)[2] * (right)[0] - (left)[0] * (right)[2]);\
	(product)[2] = ((left)[0] * (right)[1] - (left)[1] * (right)[0]);

void LALSQTPNGenerator(LALStatus *status, LALSQTPNWave *waveform, LALSQTPNWaveformParams *params) {
	INITSTATUS(status, "LALSQTPNGenerator", LALSQTPNWAVEFORMC);
	ATTATCHSTATUSPTR(status);
	ASSERT(params, status, LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
	ASSERT(waveform, status, LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
	const REAL8 geometrized_m_total = params->totalMass * LAL_MTSUN_SI;
	const REAL8 freq_Step = geometrized_m_total * LAL_PI;
	const REAL8 lengths = (5.0 / 256.0) * pow(LAL_PI, -8.0 / 3.0) * pow(params->chirpMass
			* LAL_MTSUN_SI * params->lowerFreq, -5.0 / 3.0) / params->lowerFreq;
	xlalErrno = 0;
	XLALSQTPNFillCoefficients(params);
	if (xlalErrno) {
		ABORTXLAL(status);
	}
	REAL8 valuesInitial[LALSQTPN_NUM_OF_VAR], valuesHelp[LALSQTPN_NUM_OF_VAR];
	valuesInitial[LALSQTPN_PHASE] = params->phi;
	valuesInitial[LALSQTPN_OMEGA] = params->lowerFreq * freq_Step;
	valuesInitial[LALSQTPN_LNH_1] = sin(params->inclination); ///< \f$\hat{L_N}=\sin\iota\f$
	valuesInitial[LALSQTPN_LNH_2] = 0.0; ///< \f$\hat{L_N}=0\f$
	valuesInitial[LALSQTPN_LNH_3] = cos(params->inclination); ///< \f$\hat{L_N}=\cos\iota\f$
	valuesInitial[LALSQTPN_MECO] = 0.;
	for (UINT2 i = 0; i < 3; i++) {
		valuesInitial[LALSQTPN_CHIH1_1 + i] = params->chih[0][i];
		valuesInitial[LALSQTPN_CHIH2_1 + i] = params->chih[1][i];
	}
	ark4GSLIntegrator *integrator;
	xlalErrno = 0;
	integrator = XLALAdaptiveRungeKutta4Init(LALSQTPN_NUM_OF_VAR, LALSQTPNDerivator, XLALSQTPNTest,
			1.0e-6, 1.0e-6);
	if (!integrator) {
		if (XLALClearErrno() == XLAL_ENOMEM)
			ABORT(status, LALINSPIRALH_EMEM, LALINSPIRALH_MSGEMEM);
		else
			ABORTXLAL(status);
	}
	integrator->stopontestonly = 1;
	REAL8Array *values;
	UINT4 size = XLALAdaptiveRungeKutta4(integrator, (void *)params, valuesInitial, 0.0, lengths
			/ geometrized_m_total, params->samplingTime / geometrized_m_total, &values);
	INT4 intreturn = integrator->returncode;
	XLALAdaptiveRungeKutta4Free(integrator);
	if (!size) {
		if (XLALClearErrno() == XLAL_ENOMEM) {
			ABORT(status, LALINSPIRALH_EMEM, LALINSPIRALH_MSGEMEM);
		} else {
			fprintf(stderr, "LALSQTPNWaveform: integration failed with errorcode %d.\n", intreturn);
			ABORTXLAL(status);
		}
	}
	if (intreturn && intreturn != LALSQTPN_ENERGY && intreturn != LALSQTPN_OMEGADOT) {
		fprintf(stderr, "LALSQTPNWaveform WARNING: integration terminated with code %d.\n",
				intreturn);
		fprintf(
				stderr,
				"                 Waveform parameters were m1 = %e, m2 = %e, chi1 = (%e,%e,%e), chi2 = (%e,%e,%e), inc = %e.",
				params->mass[0], params->mass[1], params->chi[0][0], params->chi[0][1],
				params->chi[0][2], params->chi[1][0], params->chi[1][1], params->chi[1][2],
				params->inclination);
	}
	if ((waveform->hp && size >= waveform->hp->length) || (waveform->waveform->f && size
			>= waveform->waveform->f->data->length)) {
		if (waveform->hp) {
			fprintf(stderr, "LALSQTPNWaveform: no space to write in signalvec1: %d vs. %d\n", size,
					waveform->hp->length);
		} else if (waveform->waveform->f) {
			fprintf(stderr, "LALQSTPNWaveform: no space to write in ff: %d vs. %d\n", size,
					waveform->waveform->f->data->length);
		} else {
			fprintf(stderr, "LALSQTPNWaveform: no space to write anywhere!\n");
		}
		ABORT(status, LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
	} else {
		REAL4 h[2];
		if (waveform->waveform->h) {
			for (UINT8 i = 0; i < size; i++) {
				for (UINT2 j = 0; j < LALSQTPN_NUM_OF_VAR; j++) {
					valuesHelp[j] = values->data[(j + 1) * size + i];
				}
				XLALSQTPNCalculateHPHC2(params, valuesHelp, &(waveform->waveform->h->data->data[2
						* i]));
			}
		}
		if (waveform->waveform->a) {
			for (UINT8 i = 0; i < size; i++) {
				for (UINT2 j = 0; j < LALSQTPN_NUM_OF_VAR; j++) {
					valuesHelp[j] = values->data[(j + 1) * size + i];
				}
				XLALSQTPNCalculateHPHC2(params, valuesHelp, h);
				waveform->waveform->a->data->data[2 * i] = h[0];
				waveform->waveform->a->data->data[2 * i + 1] = h[1];
				waveform->waveform->f->data->data[i] = valuesHelp[LALSQTPN_OMEGA] / freq_Step;
				waveform->waveform->phi->data->data[i] = LAL_PI_4;
				waveform->waveform->shift->data->data[i] = 0.0;
			}
		}
		if (waveform->hp) {
			for (UINT8 i = 0; i < size; i++) {
				for (UINT2 j = 0; j < LALSQTPN_NUM_OF_VAR; j++) {
					valuesHelp[j] = values->data[(j + 1) * size + i];
				}
				XLALSQTPNCalculateHPHC2(params, valuesHelp, h);
				waveform->hp->data[i] = h[0];
				if (waveform->hc) {
					waveform->hc->data[i] = h[1];
				}
			}
		}
	}
	params->finalFreq = valuesHelp[LALSQTPN_OMEGA] / freq_Step;
	params->coalescenceTime = values->data[size - 1];
	waveform->length = size;

	if (values) {
		XLALDestroyREAL8Array(values);
	}
	DETATCHSTATUSPTR(status);
	RETURN(status);
}

inline void XLALSQTPNFillCoefficients(LALSQTPNWaveformParams * const params) {

	// variable declaration and initialization
	REAL8 thetahat = 1039.0 / 4620.0;
	REAL8 spin_MPow2[2];
	REAL8 m_m[2] = { params->mass[1] / params->mass[0], params->mass[0] / params->mass[1] };
	REAL8 piPow2 = SQT_SQR(LAL_PI);
	REAL8 etaPow2 = SQT_SQR(params->eta);
	REAL8 etaPow3 = etaPow2 * params->eta;
	INT2 i;
	for (i = 0; i < 2; i++) {
		spin_MPow2[i] = params->chiAmp[i] * SQT_SQR(params->mass[i]) / SQT_SQR(params->totalMass);
	}

	// calculating the coefficients
	params->coeff.domegaGlobal = params->eta * 96. / 5.;
	for (i = LAL_PNORDER_NEWTONIAN; i <= (UINT2)params->order; i += 2) {
		params->coeff.meco[i] = -params->eta * (REAL8)(i + 2) / 6.0;
	}
	switch (params->order) {
	case LAL_PNORDER_THREE_POINT_FIVE:
		params->coeff.domega[LAL_PNORDER_THREE_POINT_FIVE] = (-4415.0 / 4032.0 + params->eta
				* 358675.0 / 6048.0 + etaPow2 * 91495.0 / 1512.) * LAL_PI;
	case LAL_PNORDER_THREE:
		params->coeff.domega[LAL_PNORDER_THREE] = (16447322263.0 / 139708800.0 - LAL_GAMMA * 1712.0
				/ 105.0 + piPow2 * 16.0 / 3.0) + (-273811877.0 / 1088640.0 + piPow2 * 451.0 / 48.0
				- thetahat * 88.0 / 3.0) * params->eta + etaPow2 * 541.0 / 896.0 - etaPow3 * 5605.0
				/ 2592.0;
		params->coeff.domegaLN = -856.0 / 105.0;
		params->coeff.meco[LAL_PNORDER_THREE] *= -675.0 / 64.0 + (209323.0 / 4032.0 - 205.0
				* piPow2 / 96.0 + (110.0 / 9.0) * (1987.0 / 3080.0)) * params->eta - 155.0
				* etaPow2 / 96.0 - 35.0 * etaPow3 / 5184.0;
	case LAL_PNORDER_TWO_POINT_FIVE:
		params->coeff.domega[LAL_PNORDER_TWO_POINT_FIVE] = -LAL_PI * (4159.0 + 15876.0
				* params->eta) / 672.0;
	case LAL_PNORDER_TWO:
		params->coeff.domega[LAL_PNORDER_TWO] = (34103.0 + 122949.0 * params->eta + 59472.0
				* etaPow2) / 18144.0;
		params->coeff.domegaSSselfConst = 0.0;
		params->coeff.domegaQMConst = 0.0;
		if ((params->spinInteraction & LAL_SSInter) == LAL_SSInter) {
			params->coeff.dchihSS[0] = spin_MPow2[1] / 2.0;
			params->coeff.dchihSS[1] = spin_MPow2[0] / 2.0;
			params->coeff.domegaSS[0] = 721.0 * params->eta * params->chiAmp[0] * params->chiAmp[1]
					/ 48.0;
			params->coeff.domegaSS[1] = -247.0 * params->coeff.domegaSS[0] / 721.0;
			params->coeff.mecoSS = -spin_MPow2[0] * spin_MPow2[1];
		}
		if ((params->spinInteraction & LAL_SSselfInter) == LAL_SSselfInter) {
			for (i = 0; i < 2; i++) {
				params->coeff.domegaSSself[i] = -spin_MPow2[i] * params->chiAmp[i] / 96.0;
				params->coeff.domegaSSselfConst -= 7.0 * params->coeff.domegaSSself[i];
			}
		}
		if ((params->spinInteraction & LAL_QMInter) == LAL_QMInter) {
			for (i = 0; i < 2; i++) {
				params->coeff.domegaQM[i] = spin_MPow2[i] * params->chiAmp[i]
						* params->qmParameter[i] * 7.5;
				params->coeff.domegaQMConst -= params->coeff.domegaQM[i] / 3.0;
				params->coeff.dchihQM[i] = -params->qmParameter[i] * params->eta
						* params->chiAmp[i] * 1.5;
			}
			params->coeff.mecoQM = 2.0 * params->eta;
		}
		params->coeff.meco[LAL_PNORDER_TWO] *= (1.0 / 24.0)
				* (-81.0 + 57.0 * params->eta - etaPow2);
	case LAL_PNORDER_ONE_POINT_FIVE:
		params->coeff.domega[LAL_PNORDER_ONE_POINT_FIVE] = 4.0 * LAL_PI;
		if ((params->spinInteraction & LAL_SOInter) == LAL_SOInter) {
			for (i = 0; i < 2; i++) {
				params->coeff.domegaSO[i] = -spin_MPow2[i] * (113.0 + 75.0 * m_m[i]) / 12.0;
				params->coeff.mecoSO[i] = -spin_MPow2[i] * 5.0 * params->eta * (4.0 + 3.0 * m_m[i])
						/ 9.0;
				params->coeff.dchihSO[i] = (4.0 + 3.0 * m_m[i]) * params->eta / 2.0;
			}
		}
		if (params->spinInteraction != 0) {
			for (i = 0; i < 2; i++) {
				params->coeff.dLNh[i] = -spin_MPow2[i] / params->eta;
			}
		}
	case LAL_PNORDER_ONE:
		params->coeff.domega[LAL_PNORDER_ONE] = -(743.0 + 924.0 * params->eta) / 336.0;
		params->coeff.meco[LAL_PNORDER_ONE] *= -(9.0 + params->eta) / 12.0;
	case LAL_PNORDER_HALF:
		params->coeff.domega[LAL_PNORDER_HALF] = 0.0;
	case LAL_PNORDER_NEWTONIAN:
		params->coeff.domega[LAL_PNORDER_NEWTONIAN] = 1.0;
	default:
		break;
	}
}

int XLALSQTPNTest(REAL8 t, const REAL8 values[], REAL8 dvalues[], void *param) {
	UNUSED(t);
	LALSQTPNWaveformParams *params = param;
	const REAL8 geometrized_m_total = params->totalMass * LAL_MTSUN_SI;
	const REAL8 freq_Step = geometrized_m_total * LAL_PI;
	REAL8 meco = params->coeff.meco[0] / params->coeff.variables.omegaPowi_3[1];
	for (UINT2 i = LAL_PNORDER_NEWTONIAN + 2; i <= params->order; i += 2) {
		meco += params->coeff.meco[i] * params->coeff.variables.omegaPowi_3[i - 1];
	}
	switch (params->order) {
	case LAL_PNORDER_PSEUDO_FOUR:
	case LAL_PNORDER_THREE_POINT_FIVE:
	case LAL_PNORDER_THREE:
	case LAL_PNORDER_TWO_POINT_FIVE:
	case LAL_PNORDER_TWO:
		if ((params->spinInteraction & LAL_SSInter) == LAL_SSInter) {
			meco += params->coeff.mecoSS * (params->coeff.variables.chih1chih2 - 3.0
					* params->coeff.variables.LNhchih[0] * params->coeff.variables.LNhchih[1])
					* values[LALSQTPN_OMEGA];
		}
		if ((params->spinInteraction & LAL_QMInter) == LAL_QMInter) {
			REAL8 temp = params->coeff.domegaQMConst;
			for (INT2 i = 0; i < 2; i++) {
				temp += params->coeff.domegaQM[i] * SQT_SQR(params->coeff.variables.LNhchih[i]);
			}
			meco += params->coeff.mecoQM * temp * values[LALSQTPN_OMEGA];
		}
	case LAL_PNORDER_ONE_POINT_FIVE:
		for (INT2 i = 0; i < 2; i++) {
			meco += params->coeff.mecoSO[i] * params->coeff.variables.LNhchih[i]
					* params->coeff.variables.omegaPowi_3[2];
		}
	case LAL_PNORDER_ONE:
	case LAL_PNORDER_HALF:
	case LAL_PNORDER_NEWTONIAN:
	default:
		break;
	}
	if (meco > 0.0) {
		return LALSQTPN_ENERGY;
	}
	if (dvalues[LALSQTPN_OMEGA] < 0.0) {
		return LALSQTPN_OMEGADOT;
	}
	if (values[LALSQTPN_OMEGA] / freq_Step > 0.5 * params->samplingFreq) {
		return LALSQTPN_NYQUIST;
	}
	if (isnan(values[LALSQTPN_OMEGA])) {
		return LALSQTPN_OMEGANAN;
	}
	UINT2 bool = 0;
	for (INT2 i = 0; i < LALSQTPN_NUM_OF_VAR; i++) {
		bool |= isnan(values[i]);
		bool |= isnan(dvalues[i]);
	}
	if (bool) {
		return LALSQTPN_NAN;
	}
	return GSL_SUCCESS;
}

int LALSQTPNDerivator(REAL8 t, const REAL8 values[], REAL8 dvalues[], void * param) {

	// variable declaration and initialization
	LALSQTPNWaveformParams *params = param;
	UNUSED(t);
	const REAL8 *chi_p[2] = { values + LALSQTPN_CHIH1_1, values + LALSQTPN_CHIH2_1 };
	UINT2 i; // indexes
	memset(dvalues, 0, LALSQTPN_NUM_OF_VAR * sizeof(REAL8));
	params->coeff.variables.omegaPowi_3[0] = 1.;
	params->coeff.variables.omegaPowi_3[1] = cbrt(values[LALSQTPN_OMEGA]);
	for (i = 2; i < 8; i++) {
		params->coeff.variables.omegaPowi_3[i] = params->coeff.variables.omegaPowi_3[i - 1]
				* params->coeff.variables.omegaPowi_3[1];
	}
	//REAL8 chih1chih2 = SCALAR_PRODUCT3(chi_p[0], chi_p[1]);
	params->coeff.variables.chih1chih2 = SCALAR_PRODUCT3(chi_p[0], chi_p[1]);
	for (i = 0; i < 2; i++) {
		params->coeff.variables.LNhchih[i] = SCALAR_PRODUCT3(values + LALSQTPN_LNH_1, chi_p[i]);
		VECTOR_PRODUCT3(values + LALSQTPN_LNH_1, chi_p[i], params->coeff.variables.LNhxchih[i]);
	}
	for (i = LAL_PNORDER_NEWTONIAN; i <= params->order; i++) {
		dvalues[LALSQTPN_OMEGA] += params->coeff.domega[i] * params->coeff.variables.omegaPowi_3[i];
	}
	dvalues[LALSQTPN_MECO] += params->coeff.meco[0] / params->coeff.variables.omegaPowi_3[1];
	for (i = LAL_PNORDER_NEWTONIAN + 2; i <= params->order; i += 2) {
		dvalues[LALSQTPN_MECO] += params->coeff.meco[i]
				* params->coeff.variables.omegaPowi_3[i - 1];
	}
	switch (params->order) {
	case LAL_PNORDER_THREE_POINT_FIVE:
	case LAL_PNORDER_THREE:
		dvalues[LALSQTPN_OMEGA] += params->coeff.domegaLN * log(16.0
				* params->coeff.variables.omegaPowi_3[2])
				* params->coeff.variables.omegaPowi_3[LAL_PNORDER_THREE];
	case LAL_PNORDER_TWO_POINT_FIVE:
	case LAL_PNORDER_TWO:
		if ((params->spinInteraction & LAL_SSInter) == LAL_SSInter) {
			XLALSQTPNAddSSContributions(params, values, dvalues);
		}
		if ((params->spinInteraction & LAL_SSselfInter) == LAL_SSselfInter) {
			XLALSQTPNAddSelfContributions(params, dvalues);
		}
		if ((params->spinInteraction & LAL_QMInter) == LAL_QMInter) {
			XLALSQTPNAddQMContributions(params, values, dvalues);
		}
	case LAL_PNORDER_ONE_POINT_FIVE:
		if ((params->spinInteraction & LAL_SOInter) == LAL_SOInter) {
			XLALSQTPNAddSOContributions(params, values, dvalues);
		}
		if (params->spinInteraction) {
			for (i = 0; i < 3; i++) {
				dvalues[LALSQTPN_LNH_1 + i] += (params->coeff.dLNh[0] * dvalues[LALSQTPN_CHIH1_1
						+ i] + params->coeff.dLNh[1] * dvalues[LALSQTPN_CHIH2_1 + i])
						* params->coeff.variables.omegaPowi_3[1];
			}
		}
	case LAL_PNORDER_ONE:
	case LAL_PNORDER_HALF:
	case LAL_PNORDER_NEWTONIAN:
	default:
		break;
	}
	dvalues[LALSQTPN_OMEGA] *= params->coeff.domegaGlobal * params->coeff.variables.omegaPowi_3[7]
			* params->coeff.variables.omegaPowi_3[4];
	dvalues[LALSQTPN_PHASE] = values[LALSQTPN_OMEGA];
	return GSL_SUCCESS;
}

inline void XLALSQTPNAddSSContributions(LALSQTPNWaveformParams *params, const REAL8 values[],
		REAL8 dvalues[]) {
	REAL8 chih1xchih2[2][3];
	const REAL8 *chi_p[2] = { values + LALSQTPN_CHIH1_1, values + LALSQTPN_CHIH2_1 };
	INT2 i, j, k;
	for (i = 0; i < 2; i++) {
		k = (i + 1) % 2;
		VECTOR_PRODUCT3(chi_p[k], chi_p[i], chih1xchih2[i]);
		for (j = 0; j < 3; j++) {
			dvalues[LALSQTPN_CHIH1_1 + 3 * i + j] += params->coeff.dchihSS[i] * (chih1xchih2[i][j]
					- 3.0 * params->coeff.variables.LNhchih[k]
							* params->coeff.variables.LNhxchih[i][j])
					* SQT_SQR(values[LALSQTPN_OMEGA]);
		}
	}
	dvalues[LALSQTPN_MECO] += params->coeff.mecoSS * (params->coeff.variables.chih1chih2 - 3.0
			* params->coeff.variables.LNhchih[0] * params->coeff.variables.LNhchih[1])
			* values[LALSQTPN_OMEGA];
	dvalues[LALSQTPN_OMEGA] += (params->coeff.domegaSS[0] * params->coeff.variables.LNhchih[0]
			* params->coeff.variables.LNhchih[1] + params->coeff.domegaSS[1]
			* params->coeff.variables.chih1chih2) * params->coeff.variables.omegaPowi_3[4];
}

inline void XLALSQTPNAddSelfContributions(LALSQTPNWaveformParams *params, REAL8 dvalues[]) {
	REAL8 temp = params->coeff.domegaSSselfConst;
	INT2 i;
	for (i = 0; i < 2; i++) {
		temp += params->coeff.domegaSSself[i] * SQT_SQR(params->coeff.variables.LNhchih[i]);
	}
	dvalues[LALSQTPN_OMEGA] += temp * params->coeff.variables.omegaPowi_3[4];
}

inline void XLALSQTPNAddQMContributions(LALSQTPNWaveformParams *params, const REAL8 values[],
		REAL8 dvalues[]) {
	REAL8 temp = params->coeff.domegaQMConst;
	INT2 i, j;
	for (i = 0; i < 2; i++) {
		temp += params->coeff.domegaQM[i] * SQT_SQR(params->coeff.variables.LNhchih[i]);
		for (j = 0; j < 3; j++) {
			dvalues[LALSQTPN_CHIH1_1 + 3 * i + j] += params->coeff.dchihQM[i]
					* params->coeff.variables.LNhchih[i] * params->coeff.variables.LNhxchih[i][j]
					* SQT_SQR(values[LALSQTPN_OMEGA]);
		}
	}
	dvalues[LALSQTPN_OMEGA] += temp * params->coeff.variables.omegaPowi_3[4];
	dvalues[LALSQTPN_MECO] += params->coeff.mecoQM * temp * values[LALSQTPN_OMEGA];
}

inline void XLALSQTPNAddSOContributions(LALSQTPNWaveformParams *params, const REAL8 values[],
		REAL8 dvalues[]) {
	INT2 i;
	for (i = 0; i < 2; i++) {
		dvalues[LALSQTPN_OMEGA] += params->coeff.domegaSO[i] * params->coeff.variables.LNhchih[i]
				* values[LALSQTPN_OMEGA];
		dvalues[LALSQTPN_MECO] += params->coeff.mecoSO[i] * params->coeff.variables.LNhchih[i]
				* params->coeff.variables.omegaPowi_3[2];
	}
	for (i = 0; i < 3; i++) {
		dvalues[LALSQTPN_CHIH1_1 + i] += params->coeff.dchihSO[0]
				* params->coeff.variables.LNhxchih[0][i] * params->coeff.variables.omegaPowi_3[5];
		dvalues[LALSQTPN_CHIH2_1 + i] += params->coeff.dchihSO[1]
				* params->coeff.variables.LNhxchih[1][i] * params->coeff.variables.omegaPowi_3[5];
	}
}

void XLALSQTPNCalculateHPHC2(LALSQTPNWaveformParams *params, REAL8 values[], REAL4 *h) {
	REAL8 contribution[3][2];
	h[LALSQTPN_PLUS] = h[LALSQTPN_CROSS] = 0.0;
	REAL8 twoAlpha = 2.0 * atan2(values[LALSQTPN_LNH_2], values[LALSQTPN_LNH_1]);
	INT2 i;
	REAL8 amp = -2.0 * pow(values[LALSQTPN_OMEGA], 2.0 / 3.0) * params->totalMass * params->eta
			* LAL_MRSUN_SI / params->distance;
	if ((params->amplitudeContribution & LALSQTPN_1_0) == LALSQTPN_1_0) {
		XLALSQTPNCalculateAmplitudeContribution1_0(params, values, twoAlpha,
				contribution[LAL_PNORDER_ONE]);
		for (i = LALSQTPN_PLUS; i <= LALSQTPN_CROSS; i++) {
			h[i] += contribution[LAL_PNORDER_ONE][i] * pow(values[LALSQTPN_OMEGA], 2.0 / 3.0);
		}
	}
	if ((params->amplitudeContribution & LALSQTPN_0_5) == LALSQTPN_0_5) {
		XLALSQTPNCalculateAmplitudeContribution0_5(params, values, twoAlpha,
				contribution[LAL_PNORDER_HALF]);
		for (i = LALSQTPN_PLUS; i <= LALSQTPN_CROSS; i++) {
			h[i] += contribution[LAL_PNORDER_HALF][i] * pow(values[LALSQTPN_OMEGA], 1.0 / 3.0);
		}
	}
	if ((params->amplitudeContribution & LALSQTPN_0_0) == LALSQTPN_0_0) {
		XLALSQTPNCalculateAmplitudeContribution0_0(values, twoAlpha,
				contribution[LAL_PNORDER_NEWTONIAN]);
		for (i = LALSQTPN_PLUS; i <= LALSQTPN_CROSS; i++) {
			h[i] += contribution[LAL_PNORDER_NEWTONIAN][i];
		}
	}
	for (i = LALSQTPN_PLUS; i <= LALSQTPN_CROSS; i++) {
		h[i] *= amp;
	}
}

inline void XLALSQTPNCalculateAmplitudeContribution1_0(LALSQTPNWaveformParams *params,
		REAL8 values[], REAL8 twoAlpha, REAL8 contribution[]) {
	INT2 i;
	REAL8 cos_Iota = values[LALSQTPN_LNH_3];
	REAL8 sin_Iota = sin(acos(cos_Iota));
	REAL8 sinPow2_Iota = SQT_SQR(sin_Iota);
	REAL8 contribution0[2];
	REAL8 cosine_Part[3][2];
	REAL8 sine_Part[3][2];
	REAL8 sin_Phi = sin(values[LALSQTPN_PHASE]);
	REAL8 cos_Phi = cos(values[LALSQTPN_PHASE]);
	REAL8 cos_2Phi = cos(2.0 * values[LALSQTPN_PHASE]);
	REAL8 sin_4Phi = sin(4.0 * values[LALSQTPN_PHASE]);
	REAL8 cos_4Phi = cos(4.0 * values[LALSQTPN_PHASE]);
	XLALSQTPNCalculateCoefficients0_0order(values, twoAlpha, cosine_Part[LAL_PNORDER_NEWTONIAN],
			sine_Part[LAL_PNORDER_NEWTONIAN]);
	XLALSQTPNCalculateCoefficients0_5order(values, twoAlpha, cosine_Part[LAL_PNORDER_HALF]);
	XLALSQTPNCalculateCoefficients1_0order(params, values, twoAlpha, cosine_Part[LAL_PNORDER_ONE],
			sine_Part[LAL_PNORDER_ONE]);
	XLALSQTPNCalculateAmplitudeContribution0_0(values, twoAlpha, contribution0);
	for (i = LALSQTPN_PLUS; i <= LALSQTPN_CROSS; i++) {
		contribution[i] = -8.0 / 3.0 * (1.0 - 3.0 * params->eta) * sinPow2_Iota
				* (cosine_Part[LAL_PNORDER_NEWTONIAN][i] * cos_4Phi
						+ sine_Part[LAL_PNORDER_NEWTONIAN][i] * sin_4Phi)
				+ cosine_Part[LAL_PNORDER_ONE][i] * cos_Phi + sine_Part[LAL_PNORDER_ONE][i]
				* sin_Phi + 1.0 / 6.0 * (4.0 * (1.0 - 3.0 * params->eta) * sinPow2_Iota * cos_2Phi
				* cosine_Part[LAL_PNORDER_HALF][i] - (4.0 * (1.0 - 3.0 * params->eta)
				* sinPow2_Iota + (19.0 - 3.0 * params->eta)) * contribution0[i]);
	}
}

inline void XLALSQTPNCalculateAmplitudeContribution0_5(LALSQTPNWaveformParams *params,
		REAL8 values[], REAL8 twoAlpha, REAL8 contribution[]) {
	REAL8 K[2];
	INT2 i;
	REAL8 cosine_Part[2];
	REAL8 sine_Part[2];
	REAL8 cos_Iota = values[LALSQTPN_LNH_3];
	REAL8 sin_Iota = sin(acos(cos_Iota));
	REAL8 deltaM = fabs(params->mass[0] - params->mass[1]);
	REAL8 sin_Phi = sin(values[LALSQTPN_PHASE]);
	REAL8 cos_Phi = cos(values[LALSQTPN_PHASE]);
	REAL8 sin_3Phi = sin(3.0 * values[LALSQTPN_PHASE]);
	REAL8 cos_3Phi = cos(3.0 * values[LALSQTPN_PHASE]);
	XLALSQTPNCalculateCoefficients0_0order(values, twoAlpha, cosine_Part, sine_Part);
	XLALSQTPNCalculateCoefficients0_5order(values, twoAlpha, K);
	for (i = LALSQTPN_PLUS; i <= LALSQTPN_CROSS; i++) {
		contribution[i] = 0.25 * deltaM / params->totalMass * (3.0 * cosine_Part[i] * sin_Iota
				* (3.0 * cos_3Phi - cos_Phi) + 3.0 * sine_Part[i] * sin_Iota * (3.0 * sin_3Phi
				- sin_Phi) - 2.0 * K[i] * sin_Iota * cos_Phi);
	}
}

inline void XLALSQTPNCalculateAmplitudeContribution0_0(REAL8 values[], REAL8 twoAlpha,
		REAL8 contribution[]) {
	REAL8 cosine_Part[2];
	REAL8 sine_Part[2];
	INT2 i;
	REAL8 sin_2Phi = sin(2.0 * values[LALSQTPN_PHASE]);
	REAL8 cos_2Phi = cos(2.0 * values[LALSQTPN_PHASE]);
	XLALSQTPNCalculateCoefficients0_0order(values, twoAlpha, cosine_Part, sine_Part);
	for (i = LALSQTPN_PLUS; i <= LALSQTPN_CROSS; i++) {
		contribution[i] = -2.0 * (cosine_Part[i] * cos_2Phi + sine_Part[i] * sin_2Phi);
	}
}

inline void XLALSQTPNCalculateCoefficients1_0order(LALSQTPNWaveformParams *params, REAL8 values[],
		REAL8 twoALpha, REAL8 cosine[], REAL8 sine[]) {
	REAL8 alpha = twoALpha / 2.0;
	REAL8 DELTAX = params->totalMass * (params->chi[1][0] * params->mass[1] - params->chi[0][0]
			* params->mass[0]);
	REAL8 DELTAY = params->totalMass * (params->chi[1][1] * params->mass[1] - params->chi[0][1]
			* params->mass[0]);
	cosine[LALSQTPN_PLUS] = -(DELTAY * sin(alpha) - DELTAX * cos(alpha))
			/ SQT_SQR(params->totalMass);
	cosine[LALSQTPN_CROSS] = (DELTAY * cos(alpha) + DELTAX * sin(alpha))
			/ SQT_SQR(params->totalMass);
	REAL8 cosineX = values[LALSQTPN_LNH_3] * cos(alpha);
	REAL8 sineX = values[LALSQTPN_LNH_3] * sin(alpha);
	sine[LALSQTPN_PLUS] = -(DELTAY * cosineX + DELTAX * sineX) / SQT_SQR(params->totalMass);
	sine[LALSQTPN_CROSS] = (DELTAY * sineX + DELTAX * cosineX) / SQT_SQR(params->totalMass);
}

inline void XLALSQTPNCalculateCoefficients0_5order(REAL8 values[], REAL8 twoAlpha, REAL8 cosine[]) {
	REAL8 sin_Iota = sin(acos(values[LALSQTPN_LNH_3]));
	REAL8 sinPow2_Iota = SQT_SQR(sin_Iota);
	cosine[LALSQTPN_PLUS] = -0.5 * cos(twoAlpha) * sinPow2_Iota;
	cosine[LALSQTPN_CROSS] = -0.5 * sin(twoAlpha) * sinPow2_Iota;
}

inline void XLALSQTPNCalculateCoefficients0_0order(REAL8 values[], REAL8 twoAlpha, REAL8 cosine[],
		REAL8 sine[]) {
	cosine[LALSQTPN_PLUS] = -(1.0 + SQT_SQR(values[LALSQTPN_LNH_3])) * cos(twoAlpha) / 2.0;
	cosine[LALSQTPN_CROSS] = -(1.0 + SQT_SQR(values[LALSQTPN_LNH_3])) * sin(twoAlpha) / 2.0;
	sine[LALSQTPN_PLUS] = values[LALSQTPN_LNH_3] * sin(twoAlpha);
	sine[LALSQTPN_CROSS] = -values[LALSQTPN_LNH_3] * cos(twoAlpha);
}
