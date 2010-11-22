///////////////////////////////////////////////////////////////////////////////
// LameXP - Audio Encoder Front-End
// Copyright (C) 2004-2010 LoRd_MuldeR <MuldeR2@GMX.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
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

//LameXP includes
#include "Global.h"
#include "Dialog_SplashScreen.h"
#include "Dialog_MainWindow.h"
#include "Dialog_Processing.h"
#include "Thread_Initialization.h"
#include "Thread_MessageProducer.h"
#include "Model_Settings.h"
#include "Model_FileList.h"
#include "Model_AudioFile.h"
#include "Encoder_Abstract.h"

//Qt includes
#include <QApplication>
#include <QMessageBox>
#include <QDate>
#include <QMutex>
#include <QDir>
#include <QInputDialog>

///////////////////////////////////////////////////////////////////////////////
// Main function
///////////////////////////////////////////////////////////////////////////////

int lamexp_main(int argc, char* argv[])
{
	int iResult = -1;
	bool bAccepted = true;
	
	//Init console
	lamexp_init_console(argc, argv);
	
	//LPWSTR *szArglist;
	//int nArgs;
	//szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	//
	//if(nArgs >= 2)
	//{
	//	static HANDLE hConsole = NULL;
	//	hConsole = CreateFile(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
	//	if(!SetConsoleCP(CP_UTF8))
	//	{
	//		wprintf(L"Failed to set CP !!!\n");
	//	}
	//	char buffer[4096];
	//	WideCharToMultiByte(CP_UTF8, 0, szArglist[1], -1, buffer, 4096, NULL, NULL);
	//	wprintf(L"%S\n", buffer);
	//	WriteConsoleA(hConsole, buffer, strlen(buffer), NULL, NULL);
	//}

	//Print version info
	qDebug("LameXP - Audio Encoder Front-End");
	qDebug("Version %d.%02d %s, Build %d [%s], compiled with %s", lamexp_version_major(), lamexp_version_minor(), lamexp_version_release(), lamexp_version_build(), lamexp_version_date().toString(Qt::ISODate).toLatin1().constData(), lamexp_version_compiler());
	qDebug("Copyright (C) 2004-%04d LoRd_MuldeR <MuldeR2@GMX.de>\n", max(lamexp_version_date().year(),QDate::currentDate().year()));
	
	//print license info
	qDebug("This program is free software: you can redistribute it and/or modify");
	qDebug("it under the terms of the GNU General Public License <http://www.gnu.org/>.");
	qDebug("This program comes with ABSOLUTELY NO WARRANTY.\n");

	//Print warning, if this is a "debug" build
	LAMEXP_CHECK_DEBUG_BUILD;
	
	//Detect CPU capabilities
	lamexp_cpu_t cpuFeatures = lamexp_detect_cpu_features();
	qDebug("CPU brand string  :  %s", cpuFeatures.brand);
	qDebug("   CPU signature  :  Family: %d, Model: %d, Stepping: %d", cpuFeatures.family, cpuFeatures.model, cpuFeatures.stepping);
	qDebug("CPU capabilities  :  MMX: %s, SSE: %s, SSE2: %s, SSE3: %s, SSSE3: %s, x64: %s", LAMEXP_BOOL(cpuFeatures.mmx), LAMEXP_BOOL(cpuFeatures.sse), LAMEXP_BOOL(cpuFeatures.sse2), LAMEXP_BOOL(cpuFeatures.sse3), LAMEXP_BOOL(cpuFeatures.ssse3), LAMEXP_BOOL(cpuFeatures.x64));
	qDebug("CPU no. of cores  :  %d\n", cpuFeatures.count);
	
	//Initialize Qt
	lamexp_init_qt(argc, argv);

	//Check for expiration
	if(lamexp_version_demo())
	{
		QDate expireDate = lamexp_version_date().addDays(14);
		qWarning(QString("Note: This demo (pre-release) version of LameXP will expire at %1.\n").arg(expireDate.toString(Qt::ISODate)).toLatin1().constData());
		if(QDate::currentDate() >= expireDate)
		{
			qWarning("Binary has expired !!!");
			QMessageBox::warning(NULL, "LameXP - Expired", QString("This demo (pre-release) version of LameXP has expired at %1.\nLameXP is free software and release versions won't expire.").arg(expireDate.toString()), "Exit Program");
			return 0;
		}
	}

	//Check for multiple instances of LameXP
	if((iResult = lamexp_init_ipc()) != 0)
	{
		qDebug("LameXP is already running, connecting to running instance...");
		if(iResult == 1)
		{
			MessageProducerThread *messageProducerThread = new MessageProducerThread();
			messageProducerThread->start();
			if(!messageProducerThread->wait(30000))
			{
				messageProducerThread->terminate();
				QMessageBox messageBox(QMessageBox::Critical, "LameXP", "LameXP is already running, but the running instance doesn't respond!", QMessageBox::NoButton, NULL, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint);
				messageBox.exec();
				messageProducerThread->wait();
				LAMEXP_DELETE(messageProducerThread);
				return -1;
			}
			LAMEXP_DELETE(messageProducerThread);
		}
		return 0;
	}

	//Kill application?
	for(int i = 0; i < argc; i++)
	{
		if(!_stricmp("--kill", argv[i]) || !_stricmp("--force-kill", argv[i]))
		{
			return 0;
		}
	}
	
	//Create models
	FileListModel *fileListModel = new FileListModel();
	AudioFileModel *metaInfo = new AudioFileModel();
	SettingsModel *settingsModel = new SettingsModel();
	settingsModel->validate();
	
	//Show splash screen
	InitializationThread *poInitializationThread = new InitializationThread();
	SplashScreen::showSplash(poInitializationThread);
	LAMEXP_DELETE(poInitializationThread);

	//Create main window
	MainWindow *poMainWindow = new MainWindow(fileListModel, metaInfo, settingsModel);
	
	//Main application loop
	while(bAccepted)
	{
		//Show main window
		poMainWindow->show();
		iResult = QApplication::instance()->exec();
		bAccepted = poMainWindow->isAccepted();

		//Show processing dialog
		if(bAccepted && fileListModel->rowCount() > 0)
		{
			ProcessingDialog *processingDialog = new ProcessingDialog(fileListModel, metaInfo, settingsModel);
			processingDialog->exec();
			LAMEXP_DELETE(processingDialog);
		}
	}
	
	//Free models
	LAMEXP_DELETE(poMainWindow);
	LAMEXP_DELETE(fileListModel);
	LAMEXP_DELETE(metaInfo);
	LAMEXP_DELETE(settingsModel);
	
	//Final clean-up
	qDebug("Shutting down, please wait...\n");

	//Terminate
	return iResult;
}

///////////////////////////////////////////////////////////////////////////////
// Applicaton entry point
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	try
	{
		int iResult;
		qInstallMsgHandler(lamexp_message_handler);
		LAMEXP_MEMORY_CHECK(iResult = lamexp_main(argc, argv));
		lamexp_finalization();
		return iResult;
	}
	catch(char *error)
	{
		fflush(stdout);
		fflush(stderr);
		fprintf(stderr, "\nEXCEPTION ERROR: %s\n", error);
		FatalAppExit(0, L"Unhandeled exception error, application will exit!");
		TerminateProcess(GetCurrentProcess(), -1);
	}
	catch(int error)
	{
		fflush(stdout);
		fflush(stderr);
		fprintf(stderr, "\nEXCEPTION ERROR: Error code 0x%X\n", error);
		FatalAppExit(0, L"Unhandeled exception error, application will exit!");
		TerminateProcess(GetCurrentProcess(), -1);
	}
	catch(...)
	{
		fflush(stdout);
		fflush(stderr);
		fprintf(stderr, "\nEXCEPTION ERROR !!!\n");
		FatalAppExit(0, L"Unhandeled exception error, application will exit!");
		TerminateProcess(GetCurrentProcess(), -1);
	}
}
