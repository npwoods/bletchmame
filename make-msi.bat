set PATH=%PATH%;C:\Program Files (x86)\WiX Toolset v3.10\
set PATH=%PATH%;C:\Program Files (x86)\WiX Toolset v3.11\
which candle.exe
which light.exe
candle.exe -v bletchmame.wxs -out bletchmame.wixobj > candle.err
light.exe -v -ext WixUIExtension bletchmame.wixobj -out BletchMAME.msi > light.err
dir
type candle.err
type light.err
