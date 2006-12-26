# build for the current kernel version
%define		kernelversion %(uname -r)
# if you want to build for another kernel, just replace "%(uname -r)" by the correct number.

Summary:	UNICORN ADSL modem software
Name:		unicorn
Version:	0.8.7
Release:	1
Group:		Applications/Networking
License:	Bewan
Packager:	Jean-Philippe Pick <jp@pick.eu.org>
Source:		http://www.bewan.fr/bewan/utilisateurs/telechargement/pilotes/adsl/linux/A1012-A1006-A904-A888-%{version}.tgz
BuildRoot:	%{_tmppath}/root-%{name}
BuildRequires:	kernel-source = %{kernelversion}
Requires:	kernel = %{kernelversion}

%description
UNICORN ADSL modem tools and drivers.

%package utils
Summary:	UNICORN ADSL modem tools
Group:		Applications/Networking

%description utils
UNICORN ADSL modem tools.

%package modules-%{kernelversion}
Summary:	UNICORN ADSL modem drivers
Group:          System Environment/Kernel
 
%description modules-%{kernelversion}
UNICORN ADSL modem drivers. PCI and USB version.
only for kernel version %{kernelversion} !

%prep
%setup -q -n %{name}
 
%build
make KERNEL_SOURCES=/usr/src/linux-%{kernelversion}

%install
rm -rf %{buildroot}
make install prefix=/usr DESTDIR=%{buildroot} KVERS=%{kernelversion}

%clean
rm -rf %{buildroot}

%post modules-%{kernelversion}
depmod -a -F /boot/System.map-%{kernelversion} %{kernelversion}

%postun modules-%{kernelversion}
depmod -a -F /boot/System.map-%{kernelversion} %{kernelversion}

%files modules-%{kernelversion}
%defattr(-,root,root,-)
/lib/modules/*

%files utils
%defattr(-,root,root,-)
/usr/bin/*
/usr/share/bewan_adsl_status/pixmaps/*
/usr/share/locale/*/LC_MESSAGES/*

%changelog
* Sat Apr 25 2004 Jean-Philippe Pick <jp@pick.eu.org>
- upgraded to version 0.8.7

* Mon Mar 01 2004 Jean-Philippe Pick <jp@pick.eu.org>
- initial version for 0.8.1

