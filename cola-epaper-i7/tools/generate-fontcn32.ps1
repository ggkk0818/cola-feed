param(
  [string]$SourcePath = 'C:\Users\ggkk2\Downloads\FontCN32.c',
  [string]$HeaderPath = 'C:\Code\cola-feed\cola-epaper-i7\include\modules\display\FontCN32.h',
  [string]$CppPath = 'C:\Code\cola-feed\cola-epaper-i7\src\modules\display\FontCN32.cpp'
)

$lines = Get-Content -Path $SourcePath -Encoding UTF8
$glyphs = New-Object System.Collections.Generic.List[object]
$currentChar = $null
$currentBytes = New-Object System.Collections.Generic.List[string]

$glyphWidth = 48
$glyphHeight = 62
$bytesPerRow = $glyphWidth / 8

function Get-GlyphMetrics {
  param(
    [string[]]$Bytes,
    [uint32]$CodePoint
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
    $minX = 0
    $trimmedWidth = 0
  }
  else {
    $trimmedWidth = ($maxX - $minX) + 1
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

$header = @'
#pragma once

#include <Arduino.h>
#include <pgmspace.h>

#include "modules/display/BitmapFontTypes.h"

namespace FontCN32 {

constexpr uint8_t kGlyphWidth = 48;
constexpr uint8_t kGlyphHeight = 62;
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

}  // namespace FontCN32
'@
Set-Content -Path $HeaderPath -Value $header -Encoding UTF8

$builder = New-Object System.Text.StringBuilder
[void]$builder.AppendLine('#include "modules/display/FontCN32.h"')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('namespace FontCN32 {')
[void]$builder.AppendLine('namespace {')

for ($index = 0; $index -lt $glyphs.Count; $index++) {
  $glyph = $glyphs[$index]
  if ($glyph.Bytes.Count -ne 372) {
    throw "Glyph $index ($($glyph.Char)) has $($glyph.Bytes.Count) bytes, expected 372."
  }

  [void]$builder.AppendLine(("const uint8_t glyph{0}[] PROGMEM = {{" -f $index))
  for ($offset = 0; $offset -lt $glyph.Bytes.Count; $offset += 12) {
    $endOffset = [Math]::Min($offset + 11, $glyph.Bytes.Count - 1)
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
[void]$builder.AppendLine('}  // namespace FontCN32')

Set-Content -Path $CppPath -Value $builder.ToString() -Encoding UTF8

[pscustomobject]@{
  GlyphCount = $glyphs.Count
  Header = $HeaderPath
  Source = $CppPath
} | Format-List | Out-String
