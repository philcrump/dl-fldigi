// ----------------------------------------------------------------------------
//
//      sound.cxx
//
// Copyright (C) 2006-2007
//              Dave Freese, W1HKJ
//
// Copyright (C) 2007
//              Stelios Bounanos, M0GLD
//
// This file is part of fldigi.
//
// fldigi is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// fldigi is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fldigi; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
// ----------------------------------------------------------------------------

#include <config.h>

#include "sound.h"
#include "configuration.h"
#include <FL/Fl.H>
#include "File_Selector.h"

#ifdef MIN
# undef MIN
#endif
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#ifdef MAX
# undef MAX
#endif
#define MAX(a,b) (((a) > (b)) ? (a) : (b))


cSound::cSound()
        : sample_frequency(0), txppm(progdefaults.TX_corr), rxppm(progdefaults.RX_corr),
          tx_src_state(0), tx_src_data(0), rx_src_state(0), rx_src_data(0),
          snd_buffer(0), src_buffer(0), capture(false), playback(false),
	  generate(false), ofGenerate(0), ofCapture(0), ifPlayback(0)
{ }

cSound::~cSound()
{
	if (snd_buffer) delete [] snd_buffer;
	if (src_buffer) delete [] src_buffer;
	if (tx_src_data) delete tx_src_data;
	if (rx_src_data) delete rx_src_data;
	if (rx_src_state) src_delete (rx_src_state);
	if (tx_src_state) src_delete (tx_src_state);
	delete ofGenerate;
	delete ofCapture;
	delete ifPlayback;
}

int cSound::Capture(bool on)
{
	if (on) {
		string deffilename = "./capture.wav";
		const char *formats;
		if (format_supported(SF_FORMAT_FLAC | SF_FORMAT_PCM_16))
			formats = "*.{wav,flac}";
		else
			formats = "*.wav";

		char *fn = File_Select("Capture audio file", formats, deffilename.c_str(), 0);
		if (fn) {
			int format;
			char *suffix = strrchr(fn, '.');
			if (suffix && !strcasecmp(suffix, ".flac"))
				format = SF_FORMAT_FLAC;
			else
				format = SF_FORMAT_WAV;
			ofCapture = new SndfileHandle(fn, SFM_WRITE,
						      format | SF_FORMAT_PCM_16,
						      1, sample_frequency);
			if (!*ofCapture) {
				cerr << "Could not write " << fn << endl;
				delete ofCapture;
				ofCapture = 0;
				return 0;
			}
			ofCapture->command(SFC_SET_UPDATE_HEADER_AUTO, 0, SF_TRUE);
			tag_file(ofCapture, "Captured audio");
		}
		else
			return 0;
	}
	else {
		delete ofCapture;
		ofCapture = 0;
	}
	capture = on;
	return 1;
}

int cSound::Playback(bool on)
{
	if (on) {
		string deffilename = "./playback.wav";
		const char *formats;
		if (format_supported(SF_FORMAT_FLAC | SF_FORMAT_PCM_16))
			formats = "*.{wav,flac}";
		else
			formats = "*.wav";

		char *fn = File_Select("Playback audio file", formats, deffilename.c_str(), 0);
		if (fn) {
			ifPlayback = new SndfileHandle(fn);
			if (!*ifPlayback) {
				cerr << "Could not read " << fn << endl;
				delete ifPlayback;
				ifPlayback = 0;
				return 0;
			}
			playback = true;
		}
		else
			return 0;
	}
	else {
		delete ifPlayback;
		ifPlayback = 0;
		playback = false;
	}
	return 1;
}

int cSound::Generate(bool on)
{
	if (on) {
		string deffilename = "./generate.wav";
		const char *formats;
		if (format_supported(SF_FORMAT_FLAC | SF_FORMAT_PCM_16))
			formats = "*.{wav,flac}";
		else
			formats = "*.wav";

		char *fn = File_Select("Generate audio file", formats, deffilename.c_str(), 0);
		if (fn) {
			int format;
			char *suffix = strrchr(fn, '.');
			if (suffix && !strcasecmp(suffix, ".flac"))
				format = SF_FORMAT_FLAC;
			else
				format = SF_FORMAT_WAV;
			ofGenerate = new SndfileHandle(fn, SFM_WRITE,
						       format | SF_FORMAT_PCM_16,
						       1, sample_frequency);
			if (!*ofGenerate) {
				cerr << "Could not write " << fn << endl;
				delete ofGenerate;
				ofGenerate = 0;
				return 0;
			}
			ofGenerate->command(SFC_SET_UPDATE_HEADER_AUTO, 0, SF_TRUE);
			tag_file(ofGenerate, "Generated audio");

		}
		else
			return 0;
	}
	else {
		delete ofGenerate;
		ofGenerate = 0;
	}
	generate = on;
	return 1;
}

void cSound::writeGenerate(double *buff, int count)
{
	ofGenerate->writef(buff, count);
}

void cSound::writeCapture(double *buff, int count)
{
	ofCapture->writef(buff, count);
}

int  cSound::readPlayback(double *buff, int count)
{
	sf_count_t r = ifPlayback->readf(buff, count);

	while (r < count) {
		ifPlayback->seek(0, SEEK_SET);
		r += ifPlayback->readf(buff + r, count - r);
                if (r == 0)
                        break;
        }

	return r;
}

bool cSound::format_supported(int format)
{

        SF_INFO fmt_test = { 0, sample_frequency, 2, format, 0, 0 };
        return sf_format_check(&fmt_test);
}

void cSound::tag_file(SndfileHandle *fh, const char *title)
{
       if (fh->setString(SF_STR_TITLE, title))
               return;

       fh->setString(SF_STR_COPYRIGHT, progdefaults.myName.c_str());
       fh->setString(SF_STR_SOFTWARE, PACKAGE_NAME "-" PACKAGE_VERSION);
       fh->setString(SF_STR_ARTIST, progdefaults.myCall.c_str());

       char s[64];
       snprintf(s, sizeof(s), "%s freq=%s",
                active_modem->get_mode_name(), inpFreq->value());
       fh->setString(SF_STR_COMMENT, s);

       time_t t = time(0);
       struct tm zt;
       (void)gmtime_r(&t, &zt);
       if (strftime(s, sizeof(s), "%F %Tz", &zt) > 0)
               fh->setString(SF_STR_DATE, s);
}


cSoundOSS::cSoundOSS(const char *dev ) {
	device			= dev;
	cbuff			= 0;
	try {
		Open(O_RDONLY);
		getVersion();
		getCapabilities();
		getFormats();
		Close();
	}
	catch (SndException e) {
		std::cout << e.what()
			 << " <" << device.c_str()
			 << ">" << std::endl;
    }

	try {
		int err;
		snd_buffer	= new float [2*SND_BUF_LEN];
		src_buffer	= new float [2*SND_BUF_LEN];
		cbuff 		= new unsigned char [4 * SND_BUF_LEN];
		if (!snd_buffer || !src_buffer || !cbuff)
			throw "Cannot create libsamplerate buffers";
		for (int i = 0; i < 2*SND_BUF_LEN; i++)
			snd_buffer[i] = src_buffer[i] = 0.0;
		for (int i = 0; i < 4 * SND_BUF_LEN; i++)
			cbuff[i] = 0;

		tx_src_data = new SRC_DATA;
		rx_src_data = new SRC_DATA;
		if (!tx_src_data || !rx_src_data)
			throw "Cannot create libsamplerate data structures";

		rx_src_state = src_new(SRC_SINC_FASTEST, 2, &err);
		if (rx_src_state == 0)
			throw src_strerror(err);

		tx_src_state = src_new(SRC_SINC_FASTEST, 2, &err);
		if (tx_src_state == 0)
			throw src_strerror(err);

		rx_src_data->src_ratio = 1.0/(1.0 + rxppm/1e6);
		src_set_ratio ( rx_src_state, 1.0/(1.0 + rxppm/1e6));

		tx_src_data->src_ratio = 1.0 + txppm/1e6;
		src_set_ratio ( tx_src_state, 1.0 + txppm/1e6);
	}
	catch (SndException){
		exit(1);
	};

}

cSoundOSS::~cSoundOSS()
{
	Close();
	if (cbuff) delete [] cbuff;
}

void cSoundOSS::setfragsize()
{
	int sndparam;
// Try to get ~100ms worth of samples per fragment
	sndparam = (int)log2(sample_frequency * 0.1);
// double since we are using 16 bit samples
	sndparam += 1;
// Unlimited amount of buffers for RX, four for TX
	if (mode == O_RDONLY)
		sndparam |= 0x7FFF0000;
	else
		sndparam |= 0x00040000;

	if (ioctl(device_fd, SNDCTL_DSP_SETFRAGMENT, &sndparam) < 0)
		throw errno;
}

int cSoundOSS::Open(int md, int freq)
{
	mode = md;
	try {
		device_fd = open(device.c_str(), mode, 0);
		if (device_fd == -1)
			throw SndException(errno);
		Format(AFMT_S16_LE);	// default: 16 bit little endian
//		Channels(1);			//          1 channel
		Channels(2);			//          2 channels
		Frequency(freq);
		setfragsize();
	}
	catch (...) {
		throw;
	}
	return device_fd;
}

void cSoundOSS::Close()
{
	if (device_fd == -1)
		return;
	close(device_fd);
	device_fd = -1;
}

void cSoundOSS::getVersion()
{
	version = 0;
#ifndef __FreeBSD__
 	if (ioctl(device_fd, OSS_GETVERSION, &version) == -1) {
 		version = -1;
 		throw SndException("OSS Version");
 	}
#endif
}

void cSoundOSS::getCapabilities()
{
	capability_mask = 0;
	if (ioctl(device_fd, SNDCTL_DSP_GETCAPS, &capability_mask) == -1) {
		capability_mask = 0;
		throw SndException("OSS capabilities");
	}
}

void cSoundOSS::getFormats()
{
	format_mask = 0;
	if (ioctl(device_fd, SNDCTL_DSP_GETFMTS, &format_mask) == -1) {
		format_mask = 0;
		throw SndException("OSS formats");
	}
}

void cSoundOSS::Format(int format)
{
	play_format = format;
	if (ioctl(device_fd, SNDCTL_DSP_SETFMT, &play_format) == -1) {
		device_fd = -1;
		formatok = false;
		throw SndException("Unsupported snd card format");
    }
	formatok = true;
}

void cSoundOSS::Channels(int nuchannels)
{
	channels = nuchannels;
	if (ioctl(device_fd, SNDCTL_DSP_CHANNELS, &channels) == -1) {
		device_fd = -1;
		throw "Snd card channel request failed";
	}
}

void cSoundOSS::Frequency(int frequency)
{
	sample_frequency = frequency;
	if (ioctl(device_fd, SNDCTL_DSP_SPEED, &sample_frequency) == -1) {
		device_fd = -1;
		throw SndException("Cannot set frequency");
    }
}

int cSoundOSS::BufferSize( int seconds )
{
	int bytes_per_channel = 0;
	switch (play_format) {
        case AFMT_MU_LAW:
        case AFMT_A_LAW:
        case AFMT_IMA_ADPCM:
            bytes_per_channel = 0; /* format not supported by this program */
			break;
        case AFMT_S16_BE:
        case AFMT_U16_LE:
        case AFMT_U16_BE:
        case AFMT_MPEG:
        case AFMT_S16_LE:
			bytes_per_channel = 2;
			break;
		case AFMT_U8:
        case AFMT_S8:
            bytes_per_channel = 1;
            break;
	}
  return seconds * sample_frequency * bytes_per_channel * channels;
}

bool cSoundOSS::wait_till_finished()
{
	if (ioctl(device_fd, SNDCTL_DSP_POST, 1) == -1 )
		return false;
	if (ioctl(device_fd, SNDCTL_DSP_SYNC, 0) == -1)
		return false; /* format (or ioctl()) not supported by device */
	return true; /* all sound has been played */
}

bool cSoundOSS::reset_device()
{
	if (ioctl(device_fd, SNDCTL_DSP_RESET, 0) == -1) {
		device_fd = -1;
		return false; /* format (or ioctl()) not supported by device */
    }
	return 1; /* sounddevice has been reset */
}

int cSoundOSS::Write(unsigned char *buffer, int buffersize)
{
	if (device_fd == -1)
		return -1;
	return write (device_fd, buffer, buffersize);
}

int cSoundOSS::Read(unsigned char *buffer, int buffersize)
{
	if (device_fd == -1)
		return -1;

	return read (device_fd, buffer, buffersize);
}

int cSoundOSS::Read(double *buffer, int buffersize)
{
	short int *ibuff = (short int *)cbuff;
	int numread;
	if (device_fd == -1)
		return -1;

	numread = Read(cbuff, buffersize * 4);
	for (int i = 0; i < buffersize * 2; i++)
		src_buffer[i] = ibuff[i] / MAXSC;

	for (int i = 0; i < buffersize; i++)
		buffer[i] = src_buffer[2*i];

	if (rxppm != progdefaults.RX_corr) {
		rxppm = progdefaults.RX_corr;
		rx_src_data->src_ratio = 1.0/(1.0 + rxppm/1e6);
		src_set_ratio ( rx_src_state, 1.0/(1.0 + rxppm/1e6));
	}

	if (capture) writeCapture( buffer, buffersize);

	if (playback) {
		readPlayback( buffer, buffersize);
		double vol = valRcvMixer->value();
		for (int i = 0; i < buffersize; i++)
			buffer[i] *= vol;
		return buffersize;
	}

	if (rxppm == 0)
		return buffersize;

// process using samplerate library

	rx_src_data->data_in = src_buffer;
	rx_src_data->input_frames = buffersize;
	rx_src_data->data_out = snd_buffer;
	rx_src_data->output_frames = SND_BUF_LEN;
	rx_src_data->end_of_input = 0;

	if ((numread = src_process(rx_src_state, rx_src_data)) != 0)
		return -1;

	numread = rx_src_data->output_frames_gen;

	for (int i = 0; i < numread; i++)
		buffer[i] = snd_buffer[2*i];

	return numread;

}

int cSoundOSS::write_samples(double *buf, int count)
{
	int retval;
	short int *wbuff;
	unsigned char *p;

	if (generate) writeGenerate( buf, count );

	if (device_fd == -1 || count <= 0)
		return -1;

	if (txppm != progdefaults.TX_corr) {
		txppm = progdefaults.TX_corr;
		tx_src_data->src_ratio = 1.0 + txppm/1e6;
		src_set_ratio ( tx_src_state, 1.0 + txppm/1e6);
	}

	if (txppm == 0) {
		wbuff = new short int[2*count];
		p = (unsigned char *)wbuff;
		for (int i = 0; i < count; i++) {
			wbuff[2*i] = wbuff[2*i+1] = (short int)(buf[i] * maxsc);
		}
		count *= sizeof(short int);
		retval = Write(p, 2*count);
		delete [] wbuff;
	}
	else {
		float *inbuf;
		inbuf = new float[2*count];
		int bufsize;
		for (int i = 0; i < count; i++)
			inbuf[2*i] = inbuf[2*i+1] = buf[i];
		tx_src_data->data_in = inbuf;
		tx_src_data->input_frames = count;
		tx_src_data->data_out = src_buffer;
		tx_src_data->output_frames = SND_BUF_LEN;
		tx_src_data->end_of_input = 0;

		if (src_process(tx_src_state, tx_src_data) != 0) {
			delete [] inbuf;
			return -1;
		}
		delete [] inbuf;
		bufsize = tx_src_data->output_frames_gen;
		wbuff = new short int[2*bufsize];
		p = (unsigned char *)wbuff;

		for (int i = 0; i < 2*bufsize; i++)
			wbuff[i] = (short int)(src_buffer[i] * maxsc);
		int num2write = bufsize * 2 * sizeof(short int);

		retval = Write(p, num2write);
		delete [] wbuff;
		if (retval != num2write)
			return -1;
		retval = count;
	}

	return retval;
}

int cSoundOSS::write_stereo(double *bufleft, double *bufright, int count)
{
	int retval;
	short int *wbuff;
	unsigned char *p;

	if (generate) writeGenerate( bufleft, count );

	if (device_fd == -1 || count <= 0)
		return -1;

	if (txppm != progdefaults.TX_corr) {
		txppm = progdefaults.TX_corr;
		tx_src_data->src_ratio = 1.0 + txppm/1e6;
		src_set_ratio ( tx_src_state, 1.0 + txppm/1e6);
	}

	if (txppm == 0) {
		wbuff = new short int[2*count];
		p = (unsigned char *)wbuff;
		for (int i = 0; i < count; i++) {
			wbuff[2*i] = (short int)(bufleft[i] * maxsc);
			wbuff[2*i + 1] = (short int)(bufright[i] * maxsc);
		}
		count *= sizeof(short int);
		retval = Write(p, 2*count);
		delete [] wbuff;
	}
	else {
		float *inbuf;
		inbuf = new float[2*count];
		int bufsize;
		for (int i = 0; i < count; i++) {
			inbuf[2*i] = bufleft[i];
			inbuf[2*i+1] = bufright[i];
		}
		tx_src_data->data_in = inbuf;
		tx_src_data->input_frames = count;
		tx_src_data->data_out = src_buffer;
		tx_src_data->output_frames = SND_BUF_LEN;
		tx_src_data->end_of_input = 0;

		if (src_process(tx_src_state, tx_src_data) != 0) {
			delete [] inbuf;
			return -1;
		}
		delete [] inbuf;
		bufsize = tx_src_data->output_frames_gen;
		wbuff = new short int[2*bufsize];
		p = (unsigned char *)wbuff;

		for (int i = 0; i < 2*bufsize; i++)
			wbuff[i] = (short int)(src_buffer[i] * maxsc);

		int num2write = bufsize * 2 * sizeof(short int);
		retval = Write(p, num2write);
		delete [] wbuff;
		if (retval != num2write)
			return -1;
		retval = count;
	}

	return retval;
}


#if USE_PORTAUDIO

cSoundPA::cSoundPA(const char *dev)
        : device(dev), sys(portaudio::System::instance()),
          frames_per_buffer(paFramesPerBufferUnspecified), req_sample_rate(0),
          dev_sample_rate(0), fbuf(0)
{
        rx_src_data = new SRC_DATA;
        tx_src_data = new SRC_DATA;
        if (!rx_src_data || !tx_src_data)
                throw SndException("Cannot create libsamplerate data structures");

        snd_buffer = new float[2 * SND_BUF_LEN];
        src_buffer = new float[2 * SND_BUF_LEN];
        fbuf = new float[2 * SND_BUF_LEN];
        if (!snd_buffer || !src_buffer || !fbuf)
                throw SndException("Cannot allocate libsamplerate buffers");
        memset(snd_buffer, 0, 2 * SND_BUF_LEN);
        memset(src_buffer, 0, 2 * SND_BUF_LEN);
        memset(fbuf, 0, 2 * SND_BUF_LEN);
}

cSoundPA::~cSoundPA()
{
        Close();
        delete [] fbuf;
}

int cSoundPA::Open(int mode, int freq)
{
        cerr << "cSoundPA::Open(" << mode << ", " << freq << "); req_sample_rate="
             << req_sample_rate << " sample_frequency=" << sample_frequency << endl;

        int old_sample_rate = (int)req_sample_rate;
        req_sample_rate = sample_frequency = freq;

        // Try to keep the stream open if we are using jack, or if we
        // are in full duplex mode and the sample rate has not changed.
        if (stream.isOpen()) {
                if (idev->hostApi().typeId() == paJACK) {
                        // If we have a new sample rate, we must reset the src data.
                        if (old_sample_rate != freq)
                                src_data_reset(1 << O_RDONLY | 1 << O_WRONLY);
                        return 0;
                }
                else if (idev->isFullDuplexDevice() && old_sample_rate == freq)
                        return 0;
        }

        Close();
        try {
                init_stream();
        }
        catch (const exception &e) {
                cerr << e.what() << endl;
                // make sure the stream is closed
                try {
                        stream.close();
                }
                catch (...) { }
                throw SndException(e.what());
        }

        mode = full_duplex()  ?  1 << O_RDONLY | 1 << O_WRONLY  :  1 << mode;
        if (!(mode & 1 << O_WRONLY))
            stream_params.setOutputParameters(portaudio::DirectionSpecificStreamParameters::null());
        if (!(mode & 1 << O_RDONLY))
            stream_params.setInputParameters(portaudio::DirectionSpecificStreamParameters::null());
        src_data_reset(mode);

#ifndef NDEBUG
       if (dev_sample_rate != req_sample_rate)
               cerr << "PA_debug: resampling " << dev_sample_rate
                    << " <-> " << req_sample_rate << endl;
#endif

        stream.open(stream_params);
        stream.start();

        return 0;
}

void cSoundPA::Close(void)
{
        if (stream.isOpen()) {
                stream.stop();
                stream.close();
        }
}

int cSoundPA::Read(double *buf, int count)
{
        int ncount = (int)floor(MIN(count, SND_BUF_LEN) / rx_src_data->src_ratio);

        try {
                stream.read(fbuf, ncount);
        }
        catch (const portaudio::PaException &e) {
                cerr << e.what() << endl;
                if (strstr(e.what(), "rflow"))
                        adjust_stream();
                else
                        throw SndException(e.what());
        }

	if (capture) writeCapture( buf, count);

	if (playback) {
		readPlayback( buf, count);
        double vol = valRcvMixer->value();
        for (int i = 0; i < count; i++)
            buf[i] *= vol;
		return count;
	}

        float *rbuf = fbuf;

        if (req_sample_rate != dev_sample_rate || progdefaults.RX_corr != 0) {
                resample(1 << O_RDONLY, rbuf, ncount, count);
                rbuf = rx_src_data->data_out;
                count = rx_src_data->output_frames_gen;
        }

        for (int i = 0; i < count; i++)
                buf[i] = rbuf[2*i];

        return count;
}

int cSoundPA::write_samples(double *buf, int count)
{
	if (generate) writeGenerate( buf, count );

        for (int i = 0; i < count; i++)
                fbuf[2*i] = fbuf[2*i + 1] = buf[i];

        float *wbuf = fbuf;
        if (req_sample_rate != dev_sample_rate || progdefaults.TX_corr != 0) {
                resample(1 << O_WRONLY, wbuf, count);
                wbuf = tx_src_data->data_out;
                count = tx_src_data->output_frames_gen;
        }
        try {
                stream.write(wbuf, count);
        }
        catch (const portaudio::PaException &e) {
                cerr << e.what() << endl;
                if (strstr(e.what(), "rflow"))
                        adjust_stream();
                else
                        throw SndException(e.what());
        }

        return count;
}

int cSoundPA::write_stereo(double *bufleft, double *bufright, int count)
{
	if (generate) writeGenerate( bufleft, count );

        for (int i = 0; i < count; i++) {
                fbuf[2*i] = bufleft[i];
                fbuf[2*i + 1] = bufright[i];
        }

        float *wbuf = fbuf;
        if (req_sample_rate != dev_sample_rate || progdefaults.TX_corr != 0) {
                resample(1 << O_WRONLY, wbuf, count);
                wbuf = tx_src_data->data_out;
                count = tx_src_data->output_frames_gen;
        }
        try {
                stream.write(wbuf, count);
        }
        catch (const portaudio::PaException &e) {
                cerr << e.what() << endl;
                if (strstr(e.what(), "rflow"))
                        adjust_stream();
                else
                        throw SndException(e.what());
        }

        return count;
}

bool cSoundPA::full_duplex(void)
{
        extern bool pa_allow_full_duplex;
        return pa_allow_full_duplex && idev->isFullDuplexDevice() ||
                idev->hostApi().typeId() == paJACK;
}

void cSoundPA::src_data_reset(int mode)
{
        int err;
        if (mode & 1 << O_RDONLY) {
                if (rx_src_state)
                        src_delete(rx_src_state);
                rx_src_state = src_new(SRC_SINC_FASTEST, 2, &err);
                if (!rx_src_state)
                        throw SndException(src_strerror(err));
                rx_src_data->src_ratio = req_sample_rate / (dev_sample_rate * (1.0 + rxppm / 1e6));
        }
        if (mode & 1 << O_WRONLY) {
                if (tx_src_state)
                        src_delete(tx_src_state);
                tx_src_state = src_new(SRC_SINC_FASTEST, 2, &err);
                if (!tx_src_state)
                        throw SndException(src_strerror(err));
                tx_src_data->src_ratio = dev_sample_rate * (1.0 + txppm / 1e6) / req_sample_rate;
        }
}

void cSoundPA::resample(int mode, float *buf, int count, int max)
{
        if (mode & 1 << O_RDONLY) {
                if (rxppm != progdefaults.RX_corr) {
                        rxppm = progdefaults.RX_corr;
                        rx_src_data->src_ratio = req_sample_rate
                                                 / dev_sample_rate
                                                 * (1.0 + rxppm / 1e6);
                        src_set_ratio(rx_src_state, rx_src_data->src_ratio);
                }

                rx_src_data->data_in = buf;
                rx_src_data->input_frames = count;
                rx_src_data->data_out = snd_buffer;
                rx_src_data->output_frames = max ? max : SND_BUF_LEN;
                rx_src_data->end_of_input = 0;

                src_process(rx_src_state, rx_src_data);
        }
        else if (mode & 1 << O_WRONLY) {
                if (txppm != progdefaults.TX_corr) {
                        txppm = progdefaults.TX_corr;
                        tx_src_data->src_ratio = dev_sample_rate
                                                 * (1.0 + txppm / 1e6)
                                                 / req_sample_rate;
                        src_set_ratio(tx_src_state, tx_src_data->src_ratio);
                }

                tx_src_data->data_in = buf;
                tx_src_data->input_frames = count;
                tx_src_data->data_out = src_buffer;
                tx_src_data->output_frames = max ? max : SND_BUF_LEN;
                tx_src_data->end_of_input = 0;

                src_process(tx_src_state, tx_src_data);
        }
}

void cSoundPA::init_stream(void)
{
#ifndef NDEBUG
        cerr << "PA_debug: looking for \"" << device << "\"\n";
#endif
        for (idev = sys.devicesBegin(); idev != sys.devicesEnd(); ++idev)
                if (device == idev->name())
                        break;
        if (idev == sys.devicesEnd()) {
                idev = sys.devicesBegin();
                cerr << "PA_debug: could not find device \"" << device << "\"\n";
        }
#ifndef NDEBUG
        cerr << "PA_debug: using device:"
             << "\n index: " << idev->index()
             << "\n name: " << idev->name()
             << "\n hostAPI: " << idev->hostApi().name()
             << "\n maxInputChannels: " << idev->maxInputChannels()
             << "\n maxOutputChannels: " << idev->maxOutputChannels()
             << "\n defaultLowInputLatency: " << idev->defaultLowInputLatency()
             << "\n defaultHighInputLatency: " << idev->defaultHighInputLatency()
             << "\n defaultLowOutputLatency: " << idev->defaultLowOutputLatency()
             << "\n defaultHighOutputLatency: " << idev->defaultHighOutputLatency()
             << "\n defaultSampleRate: " << idev->defaultSampleRate()
             << boolalpha
             << "\n isInputOnlyDevice: " << idev->isInputOnlyDevice()
             << "\n isOutputOnlyDevice: " << idev->isOutputOnlyDevice()
             << "\n isFullDuplexDevice: " << idev->isFullDuplexDevice()
             << "\n isSystemDefaultInputDevice: " << idev->isSystemDefaultInputDevice()
             << "\n isSystemDefaultOutputDevice: " << idev->isSystemDefaultOutputDevice()
             << "\n isHostApiDefaultInputDevice: " << idev->isHostApiDefaultInputDevice()
             << "\n isHostApiDefaultOutputDevice: " << idev->isHostApiDefaultOutputDevice()
             << "\n\n";
#endif

        in_params.setDevice(*idev);
        in_params.setNumChannels(2);
        in_params.setSampleFormat(portaudio::FLOAT32, true);
        in_params.setSuggestedLatency(idev->defaultHighInputLatency());
        in_params.setHostApiSpecificStreamInfo(NULL);

        out_params.setDevice(*idev);
        out_params.setNumChannels(2);
        out_params.setSampleFormat(portaudio::FLOAT32, true);
        out_params.setSuggestedLatency(idev->defaultHighOutputLatency());
        out_params.setHostApiSpecificStreamInfo(NULL);

        stream_params.clearFlags();
        stream_params.setInputParameters(in_params);
        stream_params.setOutputParameters(out_params);

        dev_sample_rate = get_best_srate();
        stream_params.setSampleRate(dev_sample_rate);

        max_frames_per_buffer = ceil2(MIN(SND_BUF_LEN, (unsigned)(SCBLOCKSIZE *
                                                       dev_sample_rate / req_sample_rate)));
        extern int pa_frames_per_buffer;
        if (pa_frames_per_buffer)
                frames_per_buffer = pa_frames_per_buffer;

        stream_params.setFramesPerBuffer(frames_per_buffer);
#ifndef NDEBUG
        cerr << "PA_debug: max_frames_per_buffer = " << max_frames_per_buffer << endl;
#endif
}

void cSoundPA::adjust_stream(void)
{
        if (frames_per_buffer == max_frames_per_buffer)
                return;

        frames_per_buffer = stream_params.framesPerBuffer();
        if (frames_per_buffer)
                frames_per_buffer *= 2;
        else
                frames_per_buffer = SCBLOCKSIZE;

        if (!powerof2(frames_per_buffer))
            frames_per_buffer = ceil2(frames_per_buffer);

        frames_per_buffer = MIN(max_frames_per_buffer, frames_per_buffer);

        cerr << "PA_debug: adjusting frames_per_buffer to "
             << frames_per_buffer << endl;
        stream_params.setFramesPerBuffer(frames_per_buffer);
        Close();
        stream.open(stream_params);
        stream.start();
}

double cSoundPA::std_sample_rates[] = { -1, 8000, 9600, 11025, 12000,
                                        16000, 22050, 24000, 32000,
                                        44100, 48000, 88200, 96000 };

// Return the hardware-supported sample rate that is closest to the one
// requested by the modem. This needs to be a little smarter; atm it simply
// goes through the array, and may even try the same rate twice.
double cSoundPA::get_best_srate(void)
{
        extern double pa_sample_rate;
        if (pa_sample_rate)
                return pa_sample_rate;

        int asize = sizeof(std_sample_rates) / sizeof(std_sample_rates[0]);

        std_sample_rates[0] = req_sample_rate;
        for (int i = 0; i < asize; i++) {
                portaudio::StreamParameters sp(in_params, out_params,
                                               std_sample_rates[i],
                                               0, paNoFlag);
#ifndef NDEBUG
                cerr << "PA_debug: trying " << std_sample_rates[i] << " Hz" << endl;
#endif
                if (sp.isSupported())
                        return sp.sampleRate();
        }

        throw SndException("Could not find a supported sample rate. Sound device busy?");
        return -1;
}

// Return smallest power of 2 greater than n
unsigned cSoundPA::ceil2(unsigned n)
{
        --n;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;

        return n + 1;
}

// Return the biggest power of 2 less than n
unsigned cSoundPA::floor2(unsigned n)
{
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;

        return n - (n >> 1);
}

#endif // USE_PORTAUDIO
