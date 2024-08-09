Summary: Sound Effect Generator
Name: sfxgen
Version: 0.6.0
Release: %autorelease
License: GPL-3.0-or-later
URL: https://codeberg.org/wickedsmoke/sfx_gen
Source: https://codeberg.org/wickedsmoke/sfx_gen/archive/v%{version}.tar.gz
BuildRequires: gcc-c++ qt6-qtbase-devel openal-soft-devel

%global debug_package %{nil}

%description
Sfxgen is a sound effect generator based on DrPetter's sfxr project.
A Qt based GUI program (qfxgen) and a CLI program (sfxgen) are both included
in this package.

%prep
%setup -q -n sfx_gen

%build
cc main.c -O3 -Isupport -lm -o sfxgen
qmake6 qfxgen.pro
%make_build

%install
mkdir -p %{buildroot}%{_bindir} %{buildroot}%{_datadir}/applications
install -m 755 qfxgen %{buildroot}%{_bindir}
install -m 755 sfxgen %{buildroot}%{_bindir}
install -D -m 644 dist/logo-48.png %{buildroot}%{_datadir}/icons/hicolor/48x48/apps/qfxgen.png
install -D -m 644 dist/qfxgen.desktop %{buildroot}%{_datadir}/applications

%clean
rm -rf $RPM_BUILD_ROOT

%files
%license COPYING
%defattr(-,root,root)
%{_bindir}/qfxgen
%{_bindir}/sfxgen
%{_datadir}/icons/hicolor/48x48/apps/qfxgen.png
%{_datadir}/applications/qfxgen.desktop

%changelog
* Fri Aug  9 2024 Karl Robillard <wickedsmoke@users.sf.net> 0.6.0
  - Initial package release.
