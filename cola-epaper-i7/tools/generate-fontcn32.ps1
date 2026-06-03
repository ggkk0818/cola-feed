param(
  [string]$SourcePath = 'C:\Users\ggkk2\Downloads\FontCN32.c',
  [string]$HeaderPath = 'C:\Code\cola-feed\cola-epaper-i7\include\modules\display\FontCN32.h',
  [string]$CppPath = 'C:\Code\cola-feed\cola-epaper-i7\src\modules\display\FontCN32.cpp'
)

$lines = Get-Content -Path $SourcePath -Encoding UTF8
$glyphs = New-Object System.Collections.Generic.List[object]
$currentChar = $null
$currentBytes = New-Object System.Collections.Generic.List[string]
$currentGlyphWidth = $null
$currentGlyphHeight = $null
$glyphWidth = $null
$glyphHeight = $null
$bytesPerRow = $null

$glyphHeaderPattern = [regex]'/\*--\s+.*?:\s*(.*?)\s*--\*/'
$dimensionPattern = [regex]'(\d+)\s*x\s*(\d+)'

function Complete-Glyph {
  param(
    [System.Collections.Generic.List[object]]$Glyphs,
    [string]$CurrentChar,
    [System.Collections.Generic.List[string]]$CurrentBytes,
    [Nullable[int]]$CurrentGlyphWidth,
    [Nullable[int]]$CurrentGlyphHeight,
    [ref]$GlyphWidth,
    [ref]$GlyphHeight,
    [ref]$BytesPerRow
  )

  if ($null -eq $CurrentChar -or $CurrentBytes.Count -eq 0) {
    return
  }

  if ($null -eq $CurrentGlyphWidth -or $null -eq $CurrentGlyphHeight) {
    throw "Glyph '$CurrentChar' is missing dimension metadata."
  }

  if (($CurrentGlyphWidth % 8) -ne 0) {
    throw "Glyph '$CurrentChar' width $CurrentGlyphWidth is not aligned to 8 bits."
  }

  if ($null -eq $GlyphWidth.Value) {
    $GlyphWidth.Value = $CurrentGlyphWidth
    $GlyphHeight.Value = $CurrentGlyphHeight
    $BytesPerRow.Value = [int]($CurrentGlyphWidth / 8)
  }
  elseif ($GlyphWidth.Value -ne $CurrentGlyphWidth -or $GlyphHeight.Value -ne $CurrentGlyphHeight) {
    throw "Glyph '$CurrentChar' dimensions ${CurrentGlyphWidth}x${CurrentGlyphHeight} do not match expected ${($GlyphWidth.Value)}x${($GlyphHeight.Value)}."
  }

  $expectedBytesPerGlyph = $BytesPerRow.Value * $GlyphHeight.Value
  if ($CurrentBytes.Count -ne $expectedBytesPerGlyph) {
    throw "Glyph '$CurrentChar' has $($CurrentBytes.Count) bytes, expected $expectedBytesPerGlyph for ${($GlyphWidth.Value)}x${($GlyphHeight.Value)}."
  }

  $Glyphs.Add([pscustomobject]@{ Char = $CurrentChar; Bytes = @($CurrentBytes) })
}

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
  if ($line.StartsWith('/*--')) {
    $dimensionMatch = $dimensionPattern.Match($line)
    if ($dimensionMatch.Success) {
      $currentGlyphWidth = [int]$dimensionMatch.Groups[1].Value
      $currentGlyphHeight = [int]$dimensionMatch.Groups[2].Value
    }

    $glyphHeaderMatch = $glyphHeaderPattern.Match($line)
    if ($glyphHeaderMatch.Success) {
      Complete-Glyph -Glyphs $glyphs -CurrentChar $currentChar -CurrentBytes $currentBytes `
        -CurrentGlyphWidth $currentGlyphWidth -CurrentGlyphHeight $currentGlyphHeight `
        -GlyphWidth ([ref]$glyphWidth) -GlyphHeight ([ref]$glyphHeight) -BytesPerRow ([ref]$bytesPerRow)

      $currentChar = $glyphHeaderMatch.Groups[1].Value.Trim()
      $currentBytes = New-Object System.Collections.Generic.List[string]
      $currentGlyphWidth = $null
      $currentGlyphHeight = $null
    }

    continue
  }

  foreach ($match in [regex]::Matches($line, '0x[0-9A-Fa-f]{2}')) {
    $currentBytes.Add($match.Value.ToUpperInvariant())
  }
}

Complete-Glyph -Glyphs $glyphs -CurrentChar $currentChar -CurrentBytes $currentBytes `
  -CurrentGlyphWidth $currentGlyphWidth -CurrentGlyphHeight $currentGlyphHeight `
  -GlyphWidth ([ref]$glyphWidth) -GlyphHeight ([ref]$glyphHeight) -BytesPerRow ([ref]$bytesPerRow)

if ($glyphs.Count -eq 0) {
  throw 'No glyphs parsed from font file.'
}

if ($null -eq $glyphWidth -or $null -eq $glyphHeight -or $null -eq $bytesPerRow) {
  throw 'Font dimensions could not be resolved from the source file.'
}

$header = @'
#pragma once

#include <Arduino.h>
#include <pgmspace.h>

#include "modules/display/BitmapFontTypes.h"

namespace FontCN32 {

constexpr uint8_t kGlyphWidth = __GLYPH_WIDTH__;
constexpr uint8_t kGlyphHeight = __GLYPH_HEIGHT__;
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
$header = $header.Replace('__GLYPH_WIDTH__', [string]$glyphWidth)
$header = $header.Replace('__GLYPH_HEIGHT__', [string]$glyphHeight)
Set-Content -Path $HeaderPath -Value $header -Encoding UTF8

$builder = New-Object System.Text.StringBuilder
[void]$builder.AppendLine('#include "modules/display/FontCN32.h"')
[void]$builder.AppendLine('')
[void]$builder.AppendLine('namespace FontCN32 {')
[void]$builder.AppendLine('namespace {')

for ($index = 0; $index -lt $glyphs.Count; $index++) {
  $glyph = $glyphs[$index]
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
