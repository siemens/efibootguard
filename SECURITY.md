# Security Policy

The EFI Boot Guard community takes the security of its code seriously. If you
think you have found a security vulnerability, please read the next sections
and follow the instructions to report your finding.

## Security Context

Open source software can be used in various contexts that may go far beyond
what it was originally designed and also secured for. Therefore, we describe
here how EFI Boot Guard is currently expected to be used in security-sensitive
scenarios.

Being a bootloader that can be deployed into secure boot setups, ensuring the
integrity of the security-related artifacts involved in the boot process is of
utmost importance. In scope for us is the bootloader itself, the Linux stub for
unified images provided by this project and all signed artifacts the bootloader
or the stub load and execute. All unsigned artifacts such as the EBGENV.DAT
environment files, are considered untrusted and handled accordingly in EFI Boot
Guard code.

## Reporting a Vulnerability

Please DO NOT report any potential security vulnerability via a public channel
(mailing list, github issue etc.). Instead, create a report via
https://github.com/siemens/efibootguard/security/advisories/new or contact the
maintainers jan.kiszka@siemens.com and christian.storm@siemens.com via email
directly. Please provide a detailed description of the issue, the steps to
reproduce it, the affected versions and, if already available, a proposal for a
fix. You should receive a response within 5 working days. If the issue is
confirmed as a vulnerability by us, we will open a Security Advisory on github
and give credits for your report if desired. This project follows a 90 day
disclosure timeline.
