dir "C:\Program Files (x86)"
echo $PATH
candle.exe -v bletchmame.wxs -out bletchmame.wixobj > candle.err
light.exe -v -ext WixUIExtension bletchmame.wixobj -out BletchMAME.msi > light.err
dir
type candle.err
type light.err
