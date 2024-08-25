@echo off
setlocal ENABLEDELAYEDEXPANSION

:: ---------------------------------------------------------------------------
:: SETUP ENVIRONMENT
:: ---------------------------------------------------------------------------

set "_LAMEXP_PATHS_INITIALIZED_="

call "%~dp0\_paths.bat"
call "%~dp0\_date.bat"

if "%LAMEXP_CONFIG%"=="" (
	set "LAMEXP_CONFIG=Release"
)

if "%LAMEXP_PLATFORM%"=="" (
	set "LAMEXP_PLATFORM=Win32"
)

if not "%LAMEXP_REDIST%"=="0" (
	set "LAMEXP_REDIST=1"
)

:: ---------------------------------------------------------------------------
:: SETUP PATHS
:: ---------------------------------------------------------------------------

set "BIN_PATH=%~dp0\..\..\bin\%LAMEXP_PLATFORM%\%LAMEXP_CONFIG%"
set "TMP_PATH=%TEMP%\~LameXP.%LAMEXP_CONFIG%.%ISO_DATE%.%RANDOM%.tmp"

if "%LAMEXP_SKIP_BUILD%"=="YES" (
	goto SkipBuildThisTime
)

:: ---------------------------------------------------------------------------
:: CLEAN UP
:: ---------------------------------------------------------------------------

"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" cyan "\n==========================================================================="
"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" cyan "Cleaning up..."
"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" cyan "===========================================================================\n"

for %%i in (bin,obj,tmp,ipch) do (
	del /Q /S /F "%~dp0\..\..\%%i\*.*"
)

:: ---------------------------------------------------------------------------
:: UPDATE LANGUAGE FILES AND DCOS
:: ---------------------------------------------------------------------------

call "%~dp0\_mkdocs.bat"
call "%~dp0\_lupdate.bat"

:: ---------------------------------------------------------------------------
:: BUILD THE BINARIES
:: ---------------------------------------------------------------------------

call "%~dp0\_build.bat" "%~dp0\..\..\%PATH_VCPROJ%" "%LAMEXP_PLATFORM%" "%LAMEXP_CONFIG%"

:SkipBuildThisTime

:: ---------------------------------------------------------------------------
:: READ VERSION INFO
:: ---------------------------------------------------------------------------

call "%~dp0\_version.bat"
call "%~dp0\_revision.bat"

:: ---------------------------------------------------------------------------
:: GENERATE OUTPUT FILE NAME
:: ---------------------------------------------------------------------------

mkdir "%~dp0\..\..\out" 2> NUL
set "OUT_NAME=%VER_LAMEXP_BASENAME%.%ISO_DATE%.%LAMEXP_CONFIG:_=-%.Build-%VER_LAMEXP_BUILD%"
set "OUT_FILE=%~dp0\..\..\out\!OUT_NAME!"
for /L %%n in (1, 1, 99) do (
	if exist "!OUT_FILE!.exe" set "OUT_NAME=%VER_LAMEXP_BASENAME%.%ISO_DATE%.%LAMEXP_CONFIG:_=-%.Build-%VER_LAMEXP_BUILD%.Update-%%n"
	if exist "!OUT_FILE!.zip" set "OUT_NAME=%VER_LAMEXP_BASENAME%.%ISO_DATE%.%LAMEXP_CONFIG:_=-%.Build-%VER_LAMEXP_BUILD%.Update-%%n"
	set "OUT_FILE=%~dp0\..\..\out\!OUT_NAME!"
)

:: ---------------------------------------------------------------------------
:: DELETE OLD OUTPUT FILE
:: ---------------------------------------------------------------------------

for %%i in (exe,sfx,zip,txt) do (
	del "%OUT_FILE%.%%i" 2> NUL
	if exist "%OUT_FILE%.%%i" (
		"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" red "\nFailed to delete existing output file^!\n"
		pause && exit
	)
)

:: ---------------------------------------------------------------------------
:: COPY BINARY FILES AND REDIST
:: ---------------------------------------------------------------------------

"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" cyan "\n==========================================================================="
"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" cyan "Copying binary files..."
"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" cyan "===========================================================================\n"

rd /S /Q "%TMP_PATH%" 2> NUL
mkdir "%TMP_PATH%"

if %PATH_VCTOOL% GEQ 142 (
	set "PATH_REDIST_QT=%~dp0\..\..\..\Prerequisites\Qt4\v%PATH_VCTOOL%"
	set "PATH_REDIST_VC=%~dp0\..\..\..\Prerequisites\MSVC\redist\vc\v%PATH_VCTOOL%"
) else (
	set "PATH_REDIST_QT=%~dp0\..\..\..\Prerequisites\Qt4\v%PATH_VCTOOL%_xp"
	set "PATH_REDIST_VC=%~dp0\..\..\..\Prerequisites\MSVC\redist\vc\v%PATH_VCTOOL%_xp"
)

if "%LAMEXP_CONFIG%"=="Debug" (
	set "QT_REDIST_CONFIG=Debug"
	set "QT_REDIST_SUFFIX=d"
) else (
	set "QT_REDIST_CONFIG=Shared"
	set "QT_REDIST_SUFFIX="
)

call "%~dp0\_copy.bat" "%BIN_PATH%\LameXP.exe" "%TMP_PATH%"
call "%~dp0\_copy.bat" "%~dp0\..\..\etc\Manifest\VisualElements.xml" "%TMP_PATH%\LameXP.VisualElementsManifest.xml"

if "%LAMEXP_REDIST%"=="1" (
	call "%~dp0\_copy.bat" "%BIN_PATH%\LameXP.rcc" "%TMP_PATH%"
	call "%~dp0\_copy.bat" "%BIN_PATH%\MUtils32-?.dll" "%TMP_PATH%"
	mkdir "%TMP_PATH%\imageformats"
	for %%i in (Core,Gui,Network,Xml,Svg) do (
		call "%~dp0\_copy.bat" "%PATH_REDIST_QT%\%QT_REDIST_CONFIG%\bin\Qt%%i%QT_REDIST_SUFFIX%4.dll" "%TMP_PATH%"
	)
	for %%i in (gif,ico,jpeg,mng,svg,tga,tiff) do (
		call "%~dp0\_copy.bat" "%PATH_REDIST_QT%\%QT_REDIST_CONFIG%\plugins\imageformats\q%%i%QT_REDIST_SUFFIX%4.dll" "%TMP_PATH%\imageformats"
	)
	for %%i in ("%PATH_REDIST_VC%\x86\*.dll") do call "%~dp0\_copy.bat" "%%~i" "%TMP_PATH%"
	if %PATH_VCTOOL% GEQ 140 (
		for %%i in ("%~dp0\..\..\..\Prerequisites\MSVC\redist\ucrt\DLLs\x86\*.dll") do (
			call "%~dp0\_copy.bat" "%%~i" "%TMP_PATH%"
		)
	)
)

for %%x in (exe,dll) do (
	for %%f in ("%TMP_PATH%\*.%%x") do (
		"%~dp0\..\..\..\Prerequisites\RichHeaderEraser\rchhdrrsr.exe" "%%~ff"
	)
)

if not "%LAMEXP_CONFIG%"=="Debug" (
	for %%e in (LameXP,Qt,MUtils) do (
		for %%x in (exe,dll) do (
			for %%f in ("%TMP_PATH%\%%e*.%%x") do (
				"%~dp0\..\..\..\Prerequisites\UPX\upx.exe" --best "%%~ff"
			)
		)
	)
)

call "%~dp0\_copy.bat" "%~dp0\..\..\ReadMe.txt"           "%TMP_PATH%"
call "%~dp0\_copy.bat" "%~dp0\..\..\License.txt"          "%TMP_PATH%"
call "%~dp0\_copy.bat" "%~dp0\..\..\Copying.txt"          "%TMP_PATH%"
call "%~dp0\_copy.bat" "%~dp0\..\..\doc\Changelog.html"   "%TMP_PATH%"
call "%~dp0\_copy.bat" "%~dp0\..\..\doc\Translate.html"   "%TMP_PATH%"
call "%~dp0\_copy.bat" "%~dp0\..\..\doc\Manual.html"      "%TMP_PATH%"

mkdir "%TMP_PATH%\img\lamexp"
for %%f in ("%~dp0\..\..\doc\img\lamexp\*.png") do (
	call "%~dp0\_copy.bat" "%%~f" "%TMP_PATH%\img\lamexp"
)

if not "%VER_LAMEXP_TYPE%" == "Final" (
	if not "%VER_LAMEXP_TYPE%" == "Hotfix" (
		call "%~dp0\_copy.bat" "%~dp0\..\..\doc\PRE_RELEASE_INFO.txt" "%TMP_PATH%"
	)
)

if exist "%~dp0\_postproc.bat" (
	call "%~dp0\_postproc.bat" "%TMP_PATH%"
)

echo Release: %VER_LAMEXP_MAJOR%.%VER_LAMEXP_MINOR_HI%%VER_LAMEXP_MINOR_LO%.%VER_LAMEXP_BUILD%> "%TMP_PATH%\LameXP.tag"
echo Git-%GIT_REV_NAME%-r%GIT_REV_NMBR%-%GIT_REV_HASH% [%GIT_REV_DATE%] [%GIT_REV_TIME%]>>      "%TMP_PATH%\LameXP.tag"

attrib +R "%TMP_PATH%\*.txt"
attrib +R "%TMP_PATH%\*.html"
attrib +R "%TMP_PATH%\*.exe"
attrib +R "%TMP_PATH%\*.dll"
attrib +R "%TMP_PATH%\*.tag"
attrib +R "%TMP_PATH%\*.xml"

:: ---------------------------------------------------------------------------
:: CREATE TAG
:: ---------------------------------------------------------------------------

echo LameXP - Audio Encoder Front-End> "%OUT_FILE%.txt"
echo Version %VER_LAMEXP_MAJOR%.%VER_LAMEXP_MINOR_HI%%VER_LAMEXP_MINOR_LO% %VER_LAMEXP_TYPE%-%VER_LAMEXP_PATCH%, Build #%VER_LAMEXP_BUILD% [%ISO_DATE%] [%ISO_TIME%]>> "%OUT_FILE%.txt"
echo Git-%GIT_REV_NAME%-r%GIT_REV_NMBR%-%GIT_REV_HASH% [%GIT_REV_DATE%] [%GIT_REV_TIME%]>> "%OUT_FILE%.txt"
echo.>> "%OUT_FILE%.txt"

echo ------------------------------------------------------------------------------>> "%OUT_FILE%.txt"
echo README.TXT>> "%OUT_FILE%.txt"
echo ------------------------------------------------------------------------------>> "%OUT_FILE%.txt"
echo.>> "%OUT_FILE%.txt"
"%~dp0\..\..\..\Prerequisites\MSYS\1.0\bin\cat.exe" "%~dp0\..\..\ReadMe.txt" >> "%OUT_FILE%.txt"
echo.>> "%OUT_FILE%.txt"

echo ------------------------------------------------------------------------------>> "%OUT_FILE%.txt"
echo LICENSE.TXT>> "%OUT_FILE%.txt"
echo ------------------------------------------------------------------------------>> "%OUT_FILE%.txt"
echo.>> "%OUT_FILE%.txt"
"%~dp0\..\..\..\Prerequisites\MSYS\1.0\bin\cat.exe" "%~dp0\..\..\License.txt" >> "%OUT_FILE%.txt"
echo.>> "%OUT_FILE%.txt"

:: ---------------------------------------------------------------------------
:: BUILD INSTALLER
:: ---------------------------------------------------------------------------

"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" cyan "\n==========================================================================="
"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" cyan "Creating release packages..."
"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" cyan "===========================================================================\n"

pushd "%TMP_PATH%"
"%~dp0\..\..\..\Prerequisites\InfoZip\zip.exe" -r -9 -z "%OUT_FILE%.zip" "*.*" < "%OUT_FILE%.txt"
popd

"%~dp0\..\..\..\Prerequisites\NSIS\makensis.exe" "/DLAMEXP_DATE=%ISO_DATE%" "/DLAMEXP_VERSION=%VER_LAMEXP_MAJOR%.%VER_LAMEXP_MINOR_HI%%VER_LAMEXP_MINOR_LO%" "/DLAMEXP_BUILD=%VER_LAMEXP_BUILD%" "/DLAMEXP_INSTTYPE=%VER_LAMEXP_TYPE%" "/DLAMEXP_PATCH=%VER_LAMEXP_PATCH%" "/DLAMEXP_OUTPUT_FILE=%OUT_FILE%.sfx" "/DLAMEXP_SOURCE_PATH=%TMP_PATH%" "%~dp0\..\NSIS\setup.nsi"
if %ERRORLEVEL% NEQ 0 (
	"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" red "\nFailed to build installer^!\n"
	pause && exit
)

"%~dp0\..\..\..\Prerequisites\NSIS\peheader.exe" "%OUT_FILE%.sfx"
if %ERRORLEVEL% NEQ 0 (
	"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" red "\nFailed to build installer^!\n"
	pause && exit
)

call "%~dp0\..\..\..\Prerequisites\SevenZip\7zSDex.cmd" "%OUT_FILE%.sfx" "%OUT_FILE%.exe" "LameXP Setup" "LameXP-Setup-r%VER_LAMEXP_BUILD%"
if %ERRORLEVEL% NEQ 0 (
	"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" red "\nFailed to build installer^!\n"
	pause && exit
)

set "VER_FILEVER=%VER_LAMEXP_MAJOR%.%VER_LAMEXP_MINOR_HI%.%VER_LAMEXP_MINOR_LO%.%VER_LAMEXP_PATCH%"
set "VER_PRODUCT=LameXP - Audio Encoder Front-End"
"%~dp0\..\..\..\Prerequisites\VerPatch\verpatch.exe" "%OUT_FILE%.exe" "%VER_FILEVER%" /pv "%VER_FILEVER%" /fn /s desc "%VER_PRODUCT%" /s product "%VER_PRODUCT%" /s title "LameXP Installer SFX" /s copyright "Copyright (C) LoRd_MuldeR" /s company "Free Software Foundation" /sc "Git-%GIT_REV_NAME%-r%GIT_REV_NMBR%-%GIT_REV_HASH% [%GIT_REV_DATE%] [%GIT_REV_TIME%]"
if %ERRORLEVEL% NEQ 0 (
	"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" red "\nFailed to build installer^!\n"
	pause && exit
)

"%~dp0\..\..\..\Prerequisites\NSIS\peheader.exe" "%OUT_FILE%.exe"
if %ERRORLEVEL% NEQ 0 (
	"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" red "\nFailed to build installer^!\n"
	pause && exit
)

:: ---------------------------------------------------------------------------
:: CLEAN UP
:: ---------------------------------------------------------------------------

attrib -R "%TMP_PATH%\*.txt"
attrib -R "%TMP_PATH%\*.html"
attrib -R "%TMP_PATH%\*.exe"
rd /S /Q "%TMP_PATH%"

for %%i in (zip,exe) do (
	if not exist "%OUT_FILE%.%%i" (
		"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" red "\nFailed to create release packages^!\n"
		pause && exit
	)
)

attrib +R "%OUT_FILE%.zip"
attrib +R "%OUT_FILE%.sfx"
attrib +R "%OUT_FILE%.exe"

:: ---------------------------------------------------------------------------
:: SIGN OUTPUT FILE
:: ---------------------------------------------------------------------------

"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" cyan "\n==========================================================================="
"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" cyan "Signing output file..."
"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" cyan "===========================================================================\n"

"%~dp0\..\..\..\Prerequisites\GnuPG\bin\gpg.exe" -v -u 0x6CF3FA22 -a -o "%OUT_FILE%.exe.sig"  --detach-sign "%OUT_FILE%.exe"
"%~dp0\..\..\..\Prerequisites\GnuPG\bin\gpg.exe" -v -u 0x5F57E03F -a -o "%OUT_FILE%.exe.sig2" --detach-sign "%OUT_FILE%.exe"
"%~dp0\..\..\..\Prerequisites\GnuPG\bin\gpg.exe" -v -u 0x5F57E03F -a -o "%OUT_FILE%.zip.sig2" --detach-sign "%OUT_FILE%.zip"

attrib +R "%OUT_FILE%.exe.sig"
attrib +R "%OUT_FILE%.exe.sig2"
attrib +R "%OUT_FILE%.zip.sig2"

:: ---------------------------------------------------------------------------
:: COMPLETED
:: ---------------------------------------------------------------------------

"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" green "\n==========================================================================="
"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" green "Completed successfully :-)"
"%~dp0\..\..\..\Prerequisites\CEcho\cecho.exe" green "===========================================================================\n"

pause
