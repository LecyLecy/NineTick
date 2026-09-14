param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Destination
)

Add-Type -AssemblyName PresentationCore
$decoder = [System.Windows.Media.Imaging.BitmapDecoder]::Create(
    [Uri]::new($Source),
    [System.Windows.Media.Imaging.BitmapCreateOptions]::PreservePixelFormat,
    [System.Windows.Media.Imaging.BitmapCacheOption]::OnLoad)
$scale = [System.Windows.Media.ScaleTransform]::new(256.0 / $decoder.Frames[0].PixelWidth, 256.0 / $decoder.Frames[0].PixelHeight)
$scaled = [System.Windows.Media.Imaging.TransformedBitmap]::new($decoder.Frames[0], $scale)
$encoder = [System.Windows.Media.Imaging.PngBitmapEncoder]::new()
$encoder.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($scaled))
$png = [System.IO.MemoryStream]::new()
$encoder.Save($png)
$bytes = $png.ToArray()

$stream = [System.IO.File]::Open($Destination, [System.IO.FileMode]::Create)
$writer = [System.IO.BinaryWriter]::new($stream)
$writer.Write([UInt16]0); $writer.Write([UInt16]1); $writer.Write([UInt16]1)
$writer.Write([Byte]0); $writer.Write([Byte]0); $writer.Write([Byte]0); $writer.Write([Byte]0)
$writer.Write([UInt16]1); $writer.Write([UInt16]32)
$writer.Write([UInt32]$bytes.Length); $writer.Write([UInt32]22)
$writer.Write($bytes)
$writer.Dispose(); $png.Dispose()
