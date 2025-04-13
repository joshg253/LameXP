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

#include "Global.h"

//LameXP includes
#include "LockedFile.h"

//Qt includes
#include <QApplication>
#include <QHash>
#include <QReadWriteLock>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QPair>
#include <QResource>
#include <QDir>

//MUtils
#include <MUtils/Global.h>
#include <MUtils/Exception.h>

//CRT
#include <stdint.h>

///////////////////////////////////////////////////////////////////////////////
// GLOBAL VARS
///////////////////////////////////////////////////////////////////////////////

//Typedef
typedef QPair<quint32,QString>          tool_info_t;
typedef QPair<LockedFile*, tool_info_t> tool_data_t;
typedef QHash<QString, tool_data_t>     tool_hash_t;

//Tool registry
static QScopedPointer<tool_hash_t> g_lamexp_tools_data;
static QReadWriteLock              g_lamexp_tools_lock;

//Null String
static const QString g_null_string;

//UINT_MAX
static const quint32 g_max_uint32 = UINT32_MAX;


//Resource file lock
static QScopedPointer<LockedFile> g_lamexp_rcc_lock;

//Helper Macro
#define MAKE_ENTRY(LOCK_FILE,VER,TAG) \
	qMakePair((LOCK_FILE),qMakePair((VER),(TAG)))

///////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

/*
 * Clean-up *all* registered tools
 */
static void lamexp_tools_clean_up(void)
{
	QWriteLocker writeLock(&g_lamexp_tools_lock);

	if(!g_lamexp_tools_data.isNull())
	{
		const QStringList keys = g_lamexp_tools_data->keys();
		for(QStringList::ConstIterator iter = keys.constBegin(); iter != keys.constEnd(); iter++)
		{
			tool_data_t currentTool = (*g_lamexp_tools_data)[*iter];
			MUTILS_DELETE(currentTool.first);
		}
		g_lamexp_tools_data->clear();
	}
}

/*
 * Register tool
 */
void lamexp_tools_register(const QString &toolName, LockedFile *const file, const quint32 &version, const QString &tag)
{
	QWriteLocker writeLock(&g_lamexp_tools_lock);
	
	if(!file)
	{
		MUTILS_THROW("lamexp_register_tool: Tool file must not be NULL!");
	}

	if(g_lamexp_tools_data.isNull())
	{
		g_lamexp_tools_data.reset(new tool_hash_t());
		atexit(lamexp_tools_clean_up);
	}

	const QString key = toolName.simplified().toLower();
	if(g_lamexp_tools_data->contains(key))
	{
		MUTILS_THROW("lamexp_register_tool: Tool is already registered!");
	}

	g_lamexp_tools_data->insert(key, MAKE_ENTRY(file, version, tag));
}

/*
 * Check for tool
 */
bool lamexp_tools_check(const QString &toolName)
{
	QReadLocker readLock(&g_lamexp_tools_lock);

	if(!g_lamexp_tools_data.isNull())
	{
		const QString key = toolName.simplified().toLower();
		return g_lamexp_tools_data->contains(key);
	}

	return false;
}

/*
 * Lookup tool path
 */
const QString lamexp_tools_lookup(const QString &toolName)
{
	QReadLocker readLock(&g_lamexp_tools_lock);

	if(!g_lamexp_tools_data.isNull())
	{
		const QString key = toolName.simplified().toLower();
		if(g_lamexp_tools_data->contains(key))
		{
			return (*g_lamexp_tools_data)[key].first->filePath();
		}
	}

	return g_null_string;
}

/*
 * Lookup tool version
 */
const quint32 &lamexp_tools_version(const QString &toolName, QString *const tagOut)
{
	QReadLocker readLock(&g_lamexp_tools_lock);

	if(!g_lamexp_tools_data.isNull())
	{
		const QString key = toolName.simplified().toLower();
		if(g_lamexp_tools_data->contains(key))
		{
			const tool_info_t &info = (*g_lamexp_tools_data)[key].second;
			if(tagOut)
			{
				*tagOut = info.second;
			}
			return info.first;
		}
	}

	if(tagOut)
	{
		tagOut->clear();
	}
	return g_max_uint32;
}

/*
* Initialize external resources (RCC file)
*/
void lamexp_initialize_resources(void)
{
	//Load the external RCC file
#ifndef QT_NODLL
	if (g_lamexp_rcc_lock.isNull())
	{
		const QFileInfo appPath(QCoreApplication::applicationFilePath());
		const QString rccPath = QString("%1/%2.rcc").arg(appPath.canonicalPath(), appPath.completeBaseName());
		try
		{
			qDebug("Using resource file:\n%s\n", MUTILS_UTF8(rccPath));
			g_lamexp_rcc_lock.reset(new LockedFile(rccPath));
			QResource::registerResource(g_lamexp_rcc_lock->filePath());
		}
		catch (std::runtime_error&)
		{
			qFatal("Failed to load the resource file:\n%s\n", MUTILS_UTF8(QDir::toNativeSeparators(rccPath)));
		}
	}
#endif //!QT_NODLL

	//Make sure resources are accessible!
	static const char *const RESOURCES[] = { ":/images/Logo.png", ":/tools/lame.x86-i686.exe", NULL };
	for (size_t i = 0U; RESOURCES[i]; ++i)
	{
		QResource resourceCheck(QString::fromLatin1(RESOURCES[i]));
		if ((!resourceCheck.isValid()) || (resourceCheck.size() <= 0))
		{
			qFatal("Qt resource system initialization has failed:\nResource \"%s\" not found!", RESOURCES[i]);
		}
	}
}
