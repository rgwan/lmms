/*
 * eqeffect.cpp - defination of EqEffect class.
 *
 * Copyright (c) 2014 David French <dave/dot/french3/at/googlemail/dot/com>
 *
 * This file is part of LMMS - http://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#include "EqEffect.h"
#include "embed.cpp"
#include "lmms_math.h"
#include "BasicFilters.h"
#include "interpolation.h"
#include "Engine.h"
#include "MainWindow.h"

extern "C"
{

Plugin::Descriptor PLUGIN_EXPORT eq_plugin_descriptor =
{
	STRINGIFY( PLUGIN_NAME ),
	"Equalizer",
	QT_TRANSLATE_NOOP( "pluginBrowser", "A native eq plugin" ),
	"Dave French <contact/dot/dave/dot/french3/at/googlemail/dot/com>",
	0x0100,
	Plugin::Effect,
	new PluginPixmapLoader( "logo" ),
	NULL,
	NULL
} ;

}


EqEffect::EqEffect(Model *parent, const Plugin::Descriptor::SubPluginFeatures::Key *key) :
	Effect( &eq_plugin_descriptor, parent, key ),
	m_eqControls( this ),
	m_upBufFrames( 0 )
{
	m_dFilterCount = 2;
	m_downsampleFilters = new EqLinkwitzRiley[m_dFilterCount];
	for( int i = 0; i < m_dFilterCount; i++)
	{
		m_downsampleFilters[i].setFrequency(21000);
		m_downsampleFilters[i].setSR(Engine::mixer()->processingSampleRate() * 2 );
	}
	m_upBuf = 0;
}




EqEffect::~EqEffect()
{
	if(m_upBuf)
	{
		delete m_upBuf;
	}
}




bool EqEffect::processAudioBuffer(sampleFrame *buf, const fpp_t frames)
{
	if( !isEnabled() || !isRunning () )
	{
		return( false );
	}
	m_eqControls.m_inProgress = true;
	double outSum = 0.0;
	for( fpp_t f = 0; f < frames; ++f )
	{
		outSum += buf[f][0]*buf[f][0] + buf[f][1]*buf[f][1];
	}
	const float outGain = m_eqControls.m_outGainModel.value();
	const int sampleRate = Engine::mixer()->processingSampleRate() * 2;
	sampleFrame m_inPeak = { 0, 0 };

	if(m_eqControls.m_analyzeModel.value() )
	{
		m_eqControls.m_inFftBands.analyze( buf, frames );
	}
	else
	{
		m_eqControls.m_inFftBands.clear();
	}
	upsample( buf, frames );
	gain(m_upBuf , m_upBufFrames, m_eqControls.m_inGainModel.value(), &m_inPeak );
	m_eqControls.m_inPeakL = m_eqControls.m_inPeakL < m_inPeak[0] ? m_inPeak[0] : m_eqControls.m_inPeakL;
	m_eqControls.m_inPeakR = m_eqControls.m_inPeakR < m_inPeak[1] ? m_inPeak[1] : m_eqControls.m_inPeakR;

	if(m_eqControls.m_hpActiveModel.value() ){

		m_hp12.setParameters( sampleRate, m_eqControls.m_hpFeqModel.value(), m_eqControls.m_hpResModel.value(), 1 );
		m_hp12.processBuffer( m_upBuf , m_upBufFrames );

		if( m_eqControls.m_hp24Model.value() || m_eqControls.m_hp48Model.value() )
		{
			m_hp24.setParameters( sampleRate, m_eqControls.m_hpFeqModel.value(), m_eqControls.m_hpResModel.value(), 1 );
			m_hp24.processBuffer( m_upBuf , m_upBufFrames );
		}

		if( m_eqControls.m_hp48Model.value() )
		{
			m_hp480.setParameters( sampleRate, m_eqControls.m_hpFeqModel.value(), m_eqControls.m_hpResModel.value(), 1 );
			m_hp480.processBuffer( m_upBuf , m_upBufFrames );

			m_hp481.setParameters( sampleRate, m_eqControls.m_hpFeqModel.value(), m_eqControls.m_hpResModel.value(), 1 );
			m_hp481.processBuffer( m_upBuf , m_upBufFrames );
		}
	}

	if( m_eqControls.m_lowShelfActiveModel.value() )
	{
		m_lowShelf.setParameters( sampleRate, m_eqControls.m_lowShelfFreqModel.value(), m_eqControls.m_lowShelfResModel .value(), m_eqControls.m_lowShelfGainModel.value() );
		m_lowShelf.processBuffer( m_upBuf , m_upBufFrames );
	}

	if( m_eqControls.m_para1ActiveModel.value() )
	{
		m_para1.setParameters( sampleRate, m_eqControls.m_para1FreqModel.value(), m_eqControls.m_para1ResModel.value(), m_eqControls.m_para1GainModel.value() );
		m_para1.processBuffer( m_upBuf , m_upBufFrames );
	}

	if( m_eqControls.m_para2ActiveModel.value() )
	{
		m_para2.setParameters( sampleRate, m_eqControls.m_para2FreqModel.value(), m_eqControls.m_para2ResModel.value(), m_eqControls.m_para2GainModel.value() );
		m_para2.processBuffer( m_upBuf , m_upBufFrames );
	}

	if( m_eqControls.m_para3ActiveModel.value() )
	{
		m_para3.setParameters( sampleRate, m_eqControls.m_para3FreqModel.value(), m_eqControls.m_para3ResModel.value(), m_eqControls.m_para3GainModel.value() );
		m_para3.processBuffer( m_upBuf , m_upBufFrames );
	}

	if( m_eqControls.m_para4ActiveModel.value() )
	{
		m_para4.setParameters( sampleRate, m_eqControls.m_para4FreqModel.value(), m_eqControls.m_para4ResModel.value(), m_eqControls.m_para4GainModel.value() );
		m_para4.processBuffer( m_upBuf , m_upBufFrames );
	}

	if( m_eqControls.m_highShelfActiveModel.value() )
	{
		m_highShelf.setParameters( sampleRate, m_eqControls.m_highShelfFreqModel.value(), m_eqControls.m_highShelfResModel.value(), m_eqControls.m_highShelfGainModel.value());
		m_highShelf.processBuffer( m_upBuf , m_upBufFrames );
	}

	if(m_eqControls.m_lpActiveModel.value() ){
		m_lp12.setParameters( sampleRate, m_eqControls.m_lpFreqModel.value(), m_eqControls.m_lpResModel.value(), 1 );
		m_lp12.processBuffer( m_upBuf , m_upBufFrames );

		if( m_eqControls.m_lp24Model.value() || m_eqControls.m_lp48Model.value() )
		{
			m_lp24.setParameters( sampleRate, m_eqControls.m_lpFreqModel.value(), m_eqControls.m_lpResModel.value(), 1 );
			m_lp24.processBuffer( m_upBuf , m_upBufFrames );
		}

		if( m_eqControls.m_lp48Model.value() )
		{
			m_lp480.setParameters( sampleRate, m_eqControls.m_lpFreqModel.value(), m_eqControls.m_lpResModel.value(), 1 );
			m_lp480.processBuffer( m_upBuf , m_upBufFrames );

			m_lp481.setParameters( sampleRate, m_eqControls.m_lpFreqModel.value(), m_eqControls.m_lpResModel.value(), 1 );
			m_lp481.processBuffer( m_upBuf , m_upBufFrames );
		}
	}

	sampleFrame outPeak = { 0, 0 };
	gain( m_upBuf , m_upBufFrames, outGain, &outPeak );
	m_eqControls.m_outPeakL = m_eqControls.m_outPeakL < outPeak[0] ? outPeak[0] : m_eqControls.m_outPeakL;
	m_eqControls.m_outPeakR = m_eqControls.m_outPeakR < outPeak[1] ? outPeak[1] : m_eqControls.m_outPeakR;
	for( int i =0; i < m_dFilterCount; i++)
	{
		m_downsampleFilters[i].processBuffer(m_upBuf , m_upBufFrames );
	}
	downSample( buf, frames );
	checkGate( outSum / frames );
	if(m_eqControls.m_analyzeModel.value() )
	{
		m_eqControls.m_outFftBands.analyze( buf, frames );
		setBandPeaks( &m_eqControls.m_outFftBands , ( int )( sampleRate * 0.5 ) );
	}
	else
	{
		m_eqControls.m_outFftBands.clear();
	}
	m_eqControls.m_inProgress = false;
	return isRunning();
}




float EqEffect::peakBand( float minF, float maxF, EqAnalyser *fft, int sr )
{
	float peak = -60;
	float * b = fft->m_bands;
	float h = 0;
	for(int x = 0; x < MAX_BANDS; x++, b++)
	{
		if( bandToFreq( x ,sr)  >= minF && bandToFreq( x,sr ) <= maxF )
		{
			h = 20*( log10( *b / fft->m_energy ) );
			peak = h > peak ? h : peak;
		}
	}
	return (peak+100)/100;
}

void EqEffect::setBandPeaks(EqAnalyser *fft, int samplerate )
{
	m_eqControls.m_lowShelfPeakR = m_eqControls.m_lowShelfPeakL =
			peakBand( 0,
					  m_eqControls.m_lowShelfFreqModel.value(), fft , samplerate );

	m_eqControls.m_para1PeakL = m_eqControls.m_para1PeakR =
			peakBand( m_eqControls.m_para1FreqModel.value()
					  - (m_eqControls.m_para1FreqModel.value() / m_eqControls.m_para1ResModel.value() * 0.5),
					  m_eqControls.m_para1FreqModel.value()
					  + (m_eqControls.m_para1FreqModel.value() / m_eqControls.m_para1ResModel.value() * 0.5),
					  fft , samplerate );

	m_eqControls.m_para2PeakL = m_eqControls.m_para2PeakR =
			peakBand( m_eqControls.m_para2FreqModel.value()
					  - (m_eqControls.m_para2FreqModel.value() / m_eqControls.m_para2ResModel.value() * 0.5),
					  m_eqControls.m_para2FreqModel.value()
					  + (m_eqControls.m_para2FreqModel.value() / m_eqControls.m_para2ResModel.value() * 0.5),
					  fft , samplerate );

	m_eqControls.m_para3PeakL = m_eqControls.m_para3PeakR =
			peakBand( m_eqControls.m_para3FreqModel.value()
					  - (m_eqControls.m_para3FreqModel.value() / m_eqControls.m_para3ResModel.value() * 0.5),
					  m_eqControls.m_para3FreqModel.value()
					  + (m_eqControls.m_para3FreqModel.value() / m_eqControls.m_para3ResModel.value() * 0.5),
					  fft , samplerate );

	m_eqControls.m_para4PeakL = m_eqControls.m_para4PeakR =
			peakBand( m_eqControls.m_para4FreqModel.value()
					  - (m_eqControls.m_para4FreqModel.value() / m_eqControls.m_para4ResModel.value() * 0.5),
					  m_eqControls.m_para4FreqModel.value()
					  + (m_eqControls.m_para4FreqModel.value() / m_eqControls.m_para4ResModel.value() * 0.5),
					  fft , samplerate );

	m_eqControls.m_highShelfPeakL = m_eqControls.m_highShelfPeakR =
			peakBand( m_eqControls.m_highShelfFreqModel.value(),
					  samplerate * 0.5 , fft, samplerate );
}





extern "C"
{

//needed for getting plugin out of shared lib
Plugin * PLUGIN_EXPORT lmms_plugin_main( Model* parent, void* data )
{
	return new EqEffect( parent , static_cast<const Plugin::Descriptor::SubPluginFeatures::Key *>( data ) );
}

}
