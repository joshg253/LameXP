///////////////////////////////////////////////////////////////////////////////
// LameXP - Audio Encoder Front-End
// Copyright (C) 2004-2025 LoRd_MuldeR <MuldeR2@GMX.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU GENERAL PUBLIC LICENSE as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version; always including the non-optional
// LAMEXP GNU GENERAL PUBLIC LICENSE ADDENDUM. See "License.txt" file!
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// http://www.gnu.org/licenses/gpl-2.0.txt
///////////////////////////////////////////////////////////////////////////////

#include "Encoder_FLAC.h"

#include "Global.h"
#include "Model_Settings.h"

#include <QProcess>
#include <QDir>

///////////////////////////////////////////////////////////////////////////////
// Encoder Info
///////////////////////////////////////////////////////////////////////////////

class FLACEncoderInfo : public AbstractEncoderInfo
{
public:
	virtual bool isModeSupported(int mode) const
	{
		switch(mode)
		{
		case SettingsModel::VBRMode:
			return true;
			break;
		case SettingsModel::ABRMode:
		case SettingsModel::CBRMode:
			return false;
			break;
		default:
			MUTILS_THROW("Bad RC mode specified!");
		}
	}

	virtual int valueCount(int mode) const
	{
		switch(mode)
		{
		case SettingsModel::VBRMode:
			return 9;
			break;
		case SettingsModel::ABRMode:
		case SettingsModel::CBRMode:
			return 0;
			break;
		default:
			MUTILS_THROW("Bad RC mode specified!");
		}
	}

	virtual int valueAt(int mode, int index) const
	{
		switch(mode)
		{
		case SettingsModel::VBRMode:
			return qBound(0, index, 8);
			break;
		case SettingsModel::ABRMode:
		case SettingsModel::CBRMode:
			return -1;
			break;
		default:
			MUTILS_THROW("Bad RC mode specified!");
		}
	}

	virtual int valueType(int mode) const
	{
		switch(mode)
		{
		case SettingsModel::VBRMode:
			return TYPE_COMPRESSION_LEVEL;
			break;
		case SettingsModel::ABRMode:
		case SettingsModel::CBRMode:
			return -1;
		default:
			MUTILS_THROW("Bad RC mode specified!");
		}
	}

	virtual const char *description(void) const
	{
		static const char* s_description = "Free Lossless Audio Codec (FLAC)";
		return s_description;
	}

	virtual const char *extension(void) const
	{
		static const char* s_extension = "flac";
		return s_extension;
	}

	virtual bool isResamplingSupported(void) const
	{
		return false;
	}
}
static const g_flacEncoderInfo;

///////////////////////////////////////////////////////////////////////////////
// Encoder implementation
///////////////////////////////////////////////////////////////////////////////

FLACEncoder::FLACEncoder(void)
:
	m_binary(lamexp_tools_lookup(L1S("flac.exe")))
{
	if(m_binary.isEmpty())
	{
		MUTILS_THROW("Error initializing FLAC encoder. Tool 'flac.exe' is not registred!");
	}
}

FLACEncoder::~FLACEncoder(void)
{
}

bool FLACEncoder::encode(const QString &sourceFile, const AudioFileModel_MetaInfo &metaInfo, const unsigned int /*duration*/, const unsigned int /*channels*/, const QString &outputFile, QAtomicInt &abortFlag)
{
	QProcess process;
	QStringList args;

	args << QString("-%1").arg(QString::number(qBound(0, m_configBitrate, 8)));
	args << L1S("--channel-map=none");

	if(!metaInfo.title().isEmpty())   args << L1S("-T") << QString("title=%1").arg(cleanTag(metaInfo.title()));
	if(!metaInfo.artist().isEmpty())  args << L1S("-T") << QString("artist=%1").arg(cleanTag(metaInfo.artist()));
	if(!metaInfo.album().isEmpty())   args << L1S("-T") << QString("album=%1").arg(cleanTag(metaInfo.album()));
	if(!metaInfo.genre().isEmpty())   args << L1S("-T") << QString("genre=%1").arg(cleanTag(metaInfo.genre()));
	if(!metaInfo.comment().isEmpty()) args << L1S("-T") << QString("comment=%1").arg(cleanTag(metaInfo.comment()));
	if(metaInfo.year())               args << L1S("-T") << QString("date=%1").arg(QString::number(metaInfo.year()));
	if(metaInfo.position())           args << L1S("-T") << QString("track=%1").arg(QString::number(metaInfo.position()));
	if(!metaInfo.cover().isEmpty())   args << QString("--picture=%1").arg(metaInfo.cover());

	//args << "--tv" << QString().sprintf("Encoder=LameXP v%d.%02d.%04d [%s]", lamexp_version_major(), lamexp_version_minor(), lamexp_version_build(), lamexp_version_release());

	if(!m_configCustomParams.isEmpty()) args << m_configCustomParams.split(" ", QString::SkipEmptyParts);

	args << L1S("-f") << L1S("-o") << QDir::toNativeSeparators(outputFile);
	args << QDir::toNativeSeparators(sourceFile);

	if(!startProcess(process, m_binary, args))
	{
		return false;
	}

	int prevProgress = -1;
	QRegExp regExp(L1S("\\b(\\d+)% complete"));

	const result_t result = awaitProcess(process, abortFlag, [this, &prevProgress, &regExp](const QString &text)
	{
		if (regExp.lastIndexIn(text) >= 0)
		{
			qint32 newProgress;
			if (MUtils::regexp_parse_int32(regExp, newProgress))
			{
				if (newProgress > prevProgress)
				{
					emit statusUpdated(newProgress);
					prevProgress = NEXT_PROGRESS(newProgress);
				}
			}
			return true;
		}
		return false;
	});

	return (result == RESULT_SUCCESS);
}

bool FLACEncoder::isFormatSupported(const QString &containerType, const QString& /*containerProfile*/, const QString &formatType, const QString& /*formatProfile*/, const QString& /*formatVersion*/)
{
	if(containerType.compare(L1S("Wave"), Qt::CaseInsensitive) == 0)
	{
		if(formatType.compare(L1S("PCM"), Qt::CaseInsensitive) == 0)
		{
			return true;
		}
	}

	return false;
}

const unsigned int *FLACEncoder::supportedChannelCount(void)
{
	static const unsigned int supportedChannels[] = {2, 4, 5, 6, 7, 8, NULL};
	return supportedChannels;
}

const unsigned int *FLACEncoder::supportedBitdepths(void)
{
	static const unsigned int supportedBPS[] = {16, 24, NULL};
	return supportedBPS;
}

const AbstractEncoderInfo *FLACEncoder::getEncoderInfo(void)
{
	return &g_flacEncoderInfo;
}
