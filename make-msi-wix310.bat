"c:\Program Files (x86)\WiX Toolset v3.10\bin\candle.exe" -help
"c:\Program Files (x86)\WiX Toolset v3.10\bin\light.exe" -help
"c:\Program Files (x86)\WiX Toolset v3.10\bin\candle.exe" -v bletchmame.wxs -out bletchmame.wixobj
"c:\Program Files (x86)\WiX Toolset v3.10\bin\light.exe" -v -ext WixUIExtension bletchmame.wixobj -out BletchMAME.msi > light.err
