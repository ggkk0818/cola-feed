param(
  [string]$SourcePath = 'C:\Users\ggkk2\Downloads\FontCN96.c',
  [string]$HeaderPath = 'C:\Code\cola-feed\cola-epaper-i7\include\modules\display\FontCN96.h',
  [string]$CppPath = 'C:\Code\cola-feed\cola-epaper-i7\src\modules\display\FontCN96.cpp'
)

$lines = Get-Content -Path $SourcePath -Encoding UTF8
$glyphs = New-Object System.Collections.Generic.List[object]
$currentChar = $null
$currentBytes = New-Object System.Collections.Generic.List[string]

$glyphWidth = 64
$glyphHeight = 127
$bytesPerRow = $glyphWidth / 8
$digitOneWidth = 51
$digitOneAdvance = 52
$digitOneXOffset = $glyphWidth - $digitOneWidth

function Get-GlyphBounds {
  param(
    [string[]]$Bytes
  )

  $minX = $glyphWidth
  $maxX = -1

  for ($byteOffset = 0; $byteOffset -lt $Bytes.Count; $byteOffset++) {
    $rowByte = [Convert]::ToByte($Bytes[$byteOffset], 16)
    if ($rowByte -eq 0) {
      continue
    }

    $byteIndex = $byteOffset % $bytesPerRow
    for ($bitIndex = 0; $bitIndex -lt 8; $bitIndex++) {
      if (($rowByte -band (0x80 -shr $bitIndex)) -eq 0) {
        continue
      }

      $x = ($byteIndex * 8) + $bitIndex
      if ($x -lt $minX) {
        $minX = $x
      }
      if ($x -gt $maxX) {
        $maxX = $x
      }
    }
  }

  if ($maxX -lt $minX) {
    return [pscustomobject]@{
      MinX = 0
      MaxX = -1
      Width = 0
    }
  }

  return [pscustomobject]@{
    MinX = $minX
    MaxX = $maxX
    Width = ($maxX - $minX) + 1
  }
}

function Shift-GlyphBytesRight {
  param(
    [string[]]$Bytes,
    [int]$Shift
  )

  if ($Shift -le 0) {
    return @($Bytes)
  }

  $shiftedBytes = New-Object System.Collections.Generic.List[string]

  for ($rowIndex = 0; $rowIndex -lt $glyphHeight; $rowIndex++) {
    [uint64]$rowValue = 0
    $rowOffset = $rowIndex * $bytesPerRow

    for ($byteIndex = 0; $byteIndex -lt $bytesPerRow; $byteIndex++) {
      $rowByte = [Convert]::ToByte($Bytes[$rowOffset + $byteIndex], 16)
      $rowValue = ($rowValue -shl 8) -bor [uint64]$rowByte
    }

    $rowValue = $rowValue -shr $Shift

    for ($byteIndex = 0; $byteIndex -lt $bytesPerRow; $byteIndex++) {
      $shiftBits = ($bytesPerRow - 1 - $byteIndex) * 8
      $shiftedByte = [byte](($rowValue -shr $shiftBits) -band 0xFF)
      $shiftedBytes.Add(('0X{0:X2}' -f $shiftedByte))
    }
  }

  return @($shiftedBytes)
}

function Get-GlyphMetrics {
  param(
    [string[]]$Bytes,
    [uint32]$CodePoint
  )

  $bounds = Get-GlyphBounds -Bytes $Bytes
  $minX = $bounds.MinX
  $trimmedWidth = $bounds.Width

  if ($CodePoint -eq 0x31) {
    return [pscustomobject]@{
      XOffset = $digitOneXOffset
      Width = $digitOneWidth
      Advance = $digitOneAdvance
    }
  }

  $useTrimmedMetrics = ((($CodePoint -ge 0x21) -and ($CodePoint -le 0x7E)) -or
      $CodePoint -eq 0x00B0 -or
      $CodePoint -eq 0x201C -or $CodePoint -eq 0x201D -or
      $CodePoint -eq 0x3010 -or $CodePoint -eq 0x3011) -and $trimmedWidth -gt 0

  if ($useTrimmedMetrics) {
    return [pscustomobject]@{
      XOffset = $minX
      Width = $trimmedWidth
      Advance = [Math]::Min($glyphWidth, $trimmedWidth + 1)
    }
  }

  return [pscustomobject]@{
    XOffset = 0
    Width = $glyphWidth
    Advance = $glyphWidth
  }
}

foreach ($line in $lines) {
  if ($line.StartsWith('/*--') -and $line -match ':\s*(.*?)\s*--\*/') {
    if ($null -ne $currentChar -and $currentBytes.Count -gt 0) {
      $glyphs.Add([pscustomobject]@{ Char = $currentChar; Bytes = @($currentBytes) })
    }

    $currentChar = $matches[1].Trim()
    $currentBytes = New-Object System.Collections.Generic.List[string]
    continue
  }

  foreach ($match in [regex]::Matches($line, '0x[0-9A-Fa-f]{2}')) {
    $currentBytes.Add($match.Value.ToUpperInvariant())
  }
}

if ($null -ne $currentChar -and $currentBytes.Count -gt 0) {
  $glyphs.Add([pscustomobject]@{ Char = $currentChar; Bytes = @($currentBytes) })
}

if ($glyphs.Count -eq 0) {
  throw 'No glyphs parsed from font file.'
}

for ($index = 0; $index -lt $glyphs.Count; $index++) {
  $glyph = $glyphs[$index]
  $codePoint = [System.Char]::ConvertToUtf32($glyph.Char, 0)
  if ($codePoint -ne 0x31) {
    continue
  }

  $bounds = Get-GlyphBounds -Bytes $glyph.Bytes
  if ($bounds.MaxX -lt 0) {
    continue
  }

  $targetRightEdge = $digitOneXOffset + $digitOneWidth - 1
  $shiftRight = $targetRightEdge - $bounds.MaxX
  $glyph.Bytes = @(Shift-GlyphBytesRight -Bytes $glyph.Bytes -Shift $shiftRight)
}

$header = @'
#pragma once

#include <Arduino.h>
#include <pgmspace.h>

#include "modules/display/BitmapFontTypes.h"

namespace FontCN96 {

constexpr uint8_t kGlyphWidth = 64;
constexpr uint8_t kGlyphHeight = 127;
constexpr uint8_t kBytesPerRow = kGlyphWidth / 8;
constexpr uint16_t kBytesPerGlyph = kBytesPerRow * kGlyphHeight;
constexpr int16_t kGlyphAscent = kGlyphHeight;
constexpr int16_t kGlyphDescent = 0;
constexpr int16_t kGlyphAdvance = kGlyphWidth;
constexpr int16_t kSpaceAdvance = kGlyphWidth / 2;

using Face = BitmapFont::Face;
using Glyph = BitmapFont::Glyph;

extern const Face kFace;
extern const Glyph kGlyphs[];
extern const size_t kGlyphCount;

const Glyph* findGlyph(uint32_t codePoint);

}  // namespace FontCN96
'@
Set-Content -Path $HeaderPath -Value $header -Encoding UTF8

$builder = New-Object System.Text.StringBuilder
[void]$builder.AppendLine('#include "modules/display/FontCN96.h"')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('namespace FontCN96 {')
[void]$builder.AppendLine('namespace {')

for ($index = 0; $index -lt $glyphs.Count; $index++) {
  $glyph = $glyphs[$index]
  if ($glyph.Bytes.Count -ne 1016) {
    throw "Glyph $index ($($glyph.Char)) has $($glyph.Bytes.Count) bytes, expected 1016."
  }

  [void]$builder.AppendLine(("const uint8_t glyph{0}[] PROGMEM = {{" -f $index))
  for ($offset = 0; $offset -lt $glyph.Bytes.Count; $offset += 16) {
    $endOffset = [Math]::Min($offset + 15, $glyph.Bytes.Count - 1)
    $chunk = $glyph.Bytes[$offset..$endOffset] -join ', '
    [void]$builder.AppendLine(("  {0}," -f $chunk))
  }
  [void]$builder.AppendLine('};')
  [void]$builder.AppendLine('')
}

[void]$builder.AppendLine('}  // namespace')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('const Face kFace = {')
[void]$builder.AppendLine('    kGlyphWidth,')
[void]$builder.AppendLine('    kGlyphHeight,')
[void]$builder.AppendLine('    kGlyphAscent,')
[void]$builder.AppendLine('    kGlyphDescent,')
[void]$builder.AppendLine('    kGlyphAdvance,')
[void]$builder.AppendLine('    kSpaceAdvance,')
[void]$builder.AppendLine('};')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('const Glyph kGlyphs[] = {')
for ($index = 0; $index -lt $glyphs.Count; $index++) {
  $glyph = $glyphs[$index]
  $codePoint = [System.Char]::ConvertToUtf32($glyph.Char, 0)
  $metrics = Get-GlyphMetrics -Bytes $glyph.Bytes -CodePoint $codePoint
  [void]$builder.AppendLine(("  {{0x{0:X}u, glyph{1}, {2}, {3}, {4}}}," -f $codePoint, $index, $metrics.XOffset, $metrics.Width, $metrics.Advance))
}
[void]$builder.AppendLine('};')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('const size_t kGlyphCount = sizeof(kGlyphs) / sizeof(kGlyphs[0]);')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('const Glyph* findGlyph(uint32_t codePoint) {')
[void]$builder.AppendLine('  for (size_t index = 0; index < kGlyphCount; ++index) {')
[void]$builder.AppendLine('    if (kGlyphs[index].codePoint == codePoint) {')
[void]$builder.AppendLine('      return &kGlyphs[index];')
[void]$builder.AppendLine('    }')
[void]$builder.AppendLine('  }')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('  return nullptr;')
[void]$builder.AppendLine('}')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('}  // namespace FontCN96')

Set-Content -Path $CppPath -Value $builder.ToString() -Encoding UTF8

[pscustomobject]@{
  GlyphCount = $glyphs.Count
  Header = $HeaderPath
  Source = $CppPath
} | Format-List | Out-String