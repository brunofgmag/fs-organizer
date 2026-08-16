#Requires -Version 5.1
<#
.SYNOPSIS
Builds the two user manuals into docs/ and checks that they still say the same things.

.DESCRIPTION
The sources, the figures and everything LaTeX leaves behind live here, in
manual/. What comes out of them goes to docs/, which holds the two PDFs and
nothing else: that is the folder a reader of the repository opens.

What ships in docs/ is rebuilt by the Rebuild Manual job, which wakes on the
push to develop that changes VERSION.txt, so it runs once the release has
landed. Run this one while you are writing the manual, to see what you are
writing.

It also compares the heading structure of the two languages. The manuals are two
independent sources on purpose, and nothing but this check stops one of them
from growing a section the other never got.

.PARAMETER Check
Builds nothing. Compares the heading structure of the two sources, checks that
both covers read the version off VERSION.txt, and reports whether each PDF in
docs/ is older than the .tex that makes it.
#>
[CmdletBinding()]
param(
    [switch]$Check
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$published = Join-Path (Split-Path -Parent $here) 'docs'

$languages = Get-ChildItem -LiteralPath $here -Filter 'fs-organizer-*.tex' | Sort-Object Name | ForEach-Object {
    [pscustomobject]@{ Name = $_.BaseName -replace '^fs-organizer-', ''; Source = $_.Name }
}

# One language cannot disagree with itself, so the heading comparison would pass
# by having nothing to compare. A manual deleted has to fail here, not go quiet.
if ($languages.Count -lt 2) {
    Write-Host "manual\ carries $($languages.Count) of fs-organizer-*.tex, and the heading comparison needs at least two." -ForegroundColor Red
    exit 1
}

function Get-Headings([string]$path) {
    $shape = New-Object System.Collections.Generic.List[string]

    foreach ($line in (Get-Content -LiteralPath $path -Encoding UTF8)) {
        if ($line -match '^\\(sub)?section\{') {
            $shape.Add($(if ($Matches[1]) { 'subsection' } else { 'section' }))
        }
    }

    return $shape
}

function Get-PublishedPdf([string]$source) {
    return Join-Path $published ([IO.Path]::ChangeExtension($source, '.pdf'))
}

function Test-SameShape {
    $first = $languages[0]
    $shape = Get-Headings (Join-Path $here $first.Source)

    foreach ($language in ($languages | Select-Object -Skip 1)) {
        $other = Get-Headings (Join-Path $here $language.Source)

        if ($other.Count -ne $shape.Count) {
            Write-Host "The manuals do not have the same number of headings: $($first.Name) has $($shape.Count), $($language.Name) has $($other.Count)." -ForegroundColor Red
            return $false
        }

        for ($at = 0; $at -lt $shape.Count; $at++) {
            if ($shape[$at] -ne $other[$at]) {
                Write-Host "Heading $($at + 1) is a $($shape[$at]) in $($first.Name) and a $($other[$at]) in $($language.Name)." -ForegroundColor Red
                return $false
            }
        }
    }

    Write-Host "All $($languages.Count) manuals carry the same $($shape.Count) headings, in the same order." -ForegroundColor Green
    return $true
}

function Test-SameVersion {
    $declared = (Get-Content -LiteralPath (Join-Path $here '..\VERSION.txt') -Raw).Trim()
    $agreed = $true

    foreach ($language in $languages) {
        $source = Join-Path $here $language.Source
        $typed = Select-String -LiteralPath $source -Pattern '\{(?:Version|Versão) ([0-9][^}]*)\}'
        $read = Select-String -LiteralPath $source -Pattern '\{(?:Version|Versão) \\input\{\.\./VERSION\.txt\}\}'

        if ($typed) {
            Write-Host "$($language.Source) types version $($typed.Matches.Groups[1].Value) on its cover, which goes stale on its own: the cover reads it with \input{../VERSION.txt}." -ForegroundColor Red
            $agreed = $false
        }
        elseif (-not $read) {
            Write-Host "$($language.Source) carries no version on its cover, and VERSION.txt says $declared." -ForegroundColor Red
            $agreed = $false
        }
    }

    if ($agreed) {
        Write-Host "All $($languages.Count) covers read version $declared off VERSION.txt." -ForegroundColor Green
    }

    return $agreed
}

function Test-SameFigures {
    $agreed = $true

    foreach ($language in $languages) {
        $source = Join-Path $here $language.Source
        $wanted = (Select-String -LiteralPath $source -Pattern '\\(?:shot|fsorgtitle)?\{?([0-9][0-9a-z-]*\.png)\}' -AllMatches).Matches | ForEach-Object { $_.Groups[1].Value }

        foreach ($figure in ($wanted | Select-Object -Unique)) {
            if (-not (Test-Path -LiteralPath (Join-Path $here "figures\$($language.Name)\$figure"))) {
                Write-Host "$($language.Source) asks for figures\$($language.Name)\$figure, which is not there." -ForegroundColor Red
                $agreed = $false
            }
        }
    }

    if ($agreed) {
        Write-Host 'Every figure the sources ask for is on disk.' -ForegroundColor Green
    }

    return $agreed
}

function Show-WhatIsInDocs {
    foreach ($language in $languages) {
        $source = Join-Path $here $language.Source
        $pdf = Get-PublishedPdf $language.Source

        if (-not (Test-Path -LiteralPath $pdf)) {
            Write-Host "docs\$([IO.Path]::GetFileName($pdf)) does not exist." -ForegroundColor Yellow
            continue
        }

        if ((Get-Item -LiteralPath $pdf).LastWriteTimeUtc -lt (Get-Item -LiteralPath $source).LastWriteTimeUtc) {
            Write-Host "docs\$([IO.Path]::GetFileName($pdf)) is older than its source, and the release rebuilds it." -ForegroundColor Yellow
            continue
        }

        Write-Host "docs\$([IO.Path]::GetFileName($pdf)) is newer than its source." -ForegroundColor Green
    }
}

if ($Check) {
    $shaped = Test-SameShape
    $versioned = Test-SameVersion
    $figured = Test-SameFigures

    # A date, and nothing else: git stamps every file with the checkout time, so
    # this says nothing after a clone, and between releases the source is meant
    # to be newer than the PDF the last release built.
    Show-WhatIsInDocs

    if (-not ($shaped -and $versioned -and $figured)) { exit 1 }
    exit 0
}

if (-not (Test-SameVersion)) { exit 1 }
if (-not (Test-SameFigures)) { exit 1 }

if (-not (Get-Command lualatex -ErrorAction SilentlyContinue)) {
    Write-Host 'lualatex is not on the PATH. Install a TeX distribution, or add the one you have:' -ForegroundColor Red
    Write-Host '  $env:PATH = "$env:USERPROFILE\scoop\apps\latex\current\texmfs\install\miktex\bin\x64;$env:PATH"'
    exit 1
}

New-Item -ItemType Directory -Force $published | Out-Null

Push-Location $here

try {
    foreach ($language in $languages) {
        Write-Host "==> Building $($language.Source)..."

        # Twice, because the table of contents is written on the first pass and typeset on the second.
        foreach ($pass in 1, 2) {
            & lualatex -interaction=nonstopmode $language.Source | Out-Null

            if ($LASTEXITCODE -ne 0) {
                Write-Host "lualatex failed on pass $pass. Read $([IO.Path]::ChangeExtension($language.Source, '.log'))." -ForegroundColor Red
                exit 1
            }
        }

        Move-Item -Force ([IO.Path]::ChangeExtension($language.Source, '.pdf')) (Get-PublishedPdf $language.Source)
    }
}
finally {
    Pop-Location
}

Get-ChildItem -LiteralPath $here -Include *.aux, *.out, *.toc, *.log -File -Recurse | Remove-Item -Force

foreach ($language in $languages) {
    $pdf = Get-PublishedPdf $language.Source
    $size = '{0:N0}' -f (Get-Item -LiteralPath $pdf).Length
    Write-Host "docs\$([IO.Path]::GetFileName($pdf))  $size bytes"
}

if (-not (Test-SameShape)) { exit 1 }
