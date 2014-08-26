// This code is part of DNSPing(Windows)
// DNSPing, Ping with DNS requesting.
// Copyright (C) 2014 Chengr28
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "DNSPing.h"

std::string TestDomain, TargetString, TargetDomainString;
std::wstring wTargetString, OutputFileName;
long double TotalTime = 0, MaxTime = 0, MinTime = 0;
size_t SendNum = DEFAULT_SEND_TIMES, RealSendNum = 0, RecvNum = 0, TransmissionInterval = 0, BufferSize = PACKET_MAXSIZE, RawDataLen = 0, EDNS0PayloadSize = 0;
sockaddr_storage SockAddr = {0};
uint16_t Protocol = 0, ServiceName = 0;
std::shared_ptr<char> RawData;
int SocketTimeout = DEFAULT_TIME_OUT, IP_HopLimits = 0;
auto RawSocket = false, IPv4_DF = false, EDNS0 = false;
dns_hdr HeaderParameter = {0};
dns_qry QueryParameter = {0};
dns_edns0_label EDNS0Parameter = {0};
FILE *OutputFile = nullptr;

//Main function of program
int main(int argc, char *argv[])
{
//Handle the system signal.
	if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE) == false)
	{
		wprintf(L"\nSet console ctrl handler error, error code is %lu.\n", GetLastError());
		return EXIT_FAILURE;
	}

//Winsock initialization
	WSAData WSAInitialization = {0};
	if (WSAStartup(MAKEWORD(2, 2), &WSAInitialization) != 0 || LOBYTE(WSAInitialization.wVersion) != 2 || HIBYTE(WSAInitialization.wVersion) != 2)
	{
		wprintf(L"\nWinsock initialization error, error code is %d.\n", WSAGetLastError());

		WSACleanup();
		return EXIT_FAILURE;
	}

//Main
	if (argc > 2)
	{
		std::string Parameter;
		SSIZE_T Result = 0;

	//Read parameter
		auto ReverseLookup = false, DNSSEC = false;
		for (size_t Index = 1U;Index < (size_t)argc;Index++)
		{
			Parameter = argv[Index];
			Result = 0;

		//Description(Usage)
			if (Parameter.find("?") != std::string::npos || Parameter == ("-H") || Parameter == ("-h"))
			{
				PrintDescription();
			}
		//Pings the specified host until stopped. To see statistics and continue type Control-Break. To stop type Control-C.
			else if (Parameter == ("-t"))
			{
				SendNum = 0;
			}
		//Resolve addresses to host names.
			else if (Parameter == ("-a"))
			{
				ReverseLookup = true;
			}
		//Set number of echo requests to send.
			else if (Parameter == ("-n"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result > 0 && Result <= U16_MAXNUM)
					{
						SendNum = Result;
					}
					else {
						wprintf(L"\nParameter[-n Count] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Set the "Don't Fragment" flag in outgoing packets.
			else if (Parameter == ("-f"))
			{
				IPv4_DF = true;
			}
		//Specifie a Time To Live for outgoing packets.
			else if (Parameter == ("-i"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result > 0 && Result <= U8_MAXNUM)
					{
						IP_HopLimits = (int)Result;
					}
					else {
						wprintf(L"\nParameter[-i HopLimit/TTL] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Set a long wait periods (in milliseconds) for a response.
			else if (Parameter == ("-w"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result >= TIME_OUT_MIN && Result < U16_MAXNUM)
					{
						if (IsHigherThanWin7())
							SocketTimeout = (int)Result;
						else 
							SocketTimeout = (int)(Result - 500);
					}
					else {
						wprintf(L"\nParameter[-w Timeout] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Specifie DNS header ID.
			else if (Parameter == ("-ID") || Parameter == ("-id"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result > 0 && Result <= U16_MAXNUM)
					{
						HeaderParameter.ID = htons((uint16_t)Result);
					}
					else {
						wprintf(L"\nParameter[-id DNS_ID] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Set DNS header flag: QR
			else if (Parameter == ("-QR") || Parameter == ("-qr"))
			{
				HeaderParameter.FlagsBits.QR = ~HeaderParameter.FlagsBits.QR;
			}
		//Specifie DNS header OPCode.
			else if (Parameter == ("-OPCode") || Parameter == ("-opcode"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result > 0 && Result <= U4_MAXNUM)
					{
					#if __BYTE_ORDER == __LITTLE_ENDIAN
						uint16_t TempFlags = (uint16_t)Result;
						TempFlags = htons(TempFlags << 11U);
						HeaderParameter.Flags = HeaderParameter.Flags | TempFlags;
					#else //Big-Endian
						uint8_t TempFlags = (uint8_t)Result;
						TempFlags = TempFlags & 15;//0x00001111
						HeaderParameter.FlagsBits.OPCode = TempFlags;
					#endif
					}
					else {
						wprintf(L"\nParameter[-opcode OPCode] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Set DNS header flag: AA
			else if (Parameter == ("-AA") || Parameter == ("-aa"))
			{
				HeaderParameter.FlagsBits.AA = ~HeaderParameter.FlagsBits.AA;
			}
		//Set DNS header flag: TC
			else if (Parameter == ("-TC") || Parameter == ("-tc"))
			{
				HeaderParameter.FlagsBits.TC = ~HeaderParameter.FlagsBits.TC;
			}
		//Set DNS header flag: RD
			else if (Parameter == ("-RD") || Parameter == ("-rd"))
			{
				HeaderParameter.FlagsBits.RD = ~HeaderParameter.FlagsBits.RD;
			}
		//Set DNS header flag: RA
			else if (Parameter == ("-RA") || Parameter == ("-ra"))
			{
				HeaderParameter.FlagsBits.RA = ~HeaderParameter.FlagsBits.RA;
			}
		//Set DNS header flag: AD
			else if (Parameter == ("-AD") || Parameter == ("-ad"))
			{
				HeaderParameter.FlagsBits.AD = ~HeaderParameter.FlagsBits.AD;
			}
		//Set DNS header flag: CD
			else if (Parameter == ("-CD") || Parameter == ("-cd"))
			{
				HeaderParameter.FlagsBits.CD = ~HeaderParameter.FlagsBits.CD;
			}
		//Specifie DNS header RCode.
			else if (Parameter == ("-RCode") || Parameter == ("-rcode"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result > 0 && Result <= U4_MAXNUM)
					{
					#if __BYTE_ORDER == __LITTLE_ENDIAN
						uint16_t TempFlags = (uint16_t)Result;
						TempFlags = htons(TempFlags);
						HeaderParameter.Flags = HeaderParameter.Flags | TempFlags;
					#else //Big-Endian
						uint8_t TempFlags = (uint8_t)Result;
						TempFlags = TempFlags & 15; //0x00001111
						HeaderParameter.FlagsBits.RCode = TempFlags;
					#endif
					}
					else {
						wprintf(L"\nParameter[-rcode RCode] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Specifie DNS header question count.
			else if (Parameter == ("-QN") || Parameter == ("-qn"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result > 0 && Result <= U16_MAXNUM)
					{
						HeaderParameter.Questions = htons((uint16_t)Result);
					}
					else {
						wprintf(L"\nParameter[-qn Count] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Specifie DNS header Answer count.
			else if (Parameter == ("-ANN") || Parameter == ("-ann"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result > 0 && Result <= U16_MAXNUM)
					{
						HeaderParameter.Answer = htons((uint16_t)Result);
					}
					else {
						wprintf(L"\nParameter[-ann Count] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Specifie DNS header Authority count.
			else if (Parameter == ("-AUN") || Parameter == ("-aun"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result > 0 && Result <= U16_MAXNUM)
					{
						HeaderParameter.Authority = htons((uint16_t)Result);
					}
					else {
						wprintf(L"\nParameter[-aun Count] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Specifie DNS header Additional count.
			else if (Parameter == ("-ADN") || Parameter == ("-adn"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result > 0 && Result <= U16_MAXNUM)
					{
						HeaderParameter.Additional = htons((uint16_t)Result);
					}
					else {
						wprintf(L"\nParameter[-adn Count] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Specifie transmission interval time(in milliseconds).
			else if (Parameter == ("-Ti") || Parameter == ("-ti"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result >= 0)
					{
						TransmissionInterval = Result;
					}
					else {
						wprintf(L"\nParameter[-ti IntervalTime] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Send with EDNS0 Label.
			else if (Parameter == ("-EDNS0") || Parameter == ("-edns0"))
			{
				EDNS0 = true;
			}
		//Specifie EDNS0 Label UDP Payload length.
			else if (Parameter == ("-Payload") || Parameter == ("-payload"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result > OLD_DNS_MAXSIZE && Result <= U16_MAXNUM)
					{
						EDNS0PayloadSize = Result;
					}
					else {
						wprintf(L"\nParameter[-payload Length] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}

				EDNS0 = true;
			}
		//Send with DNSSEC requesting.
			else if (Parameter == ("-DNSSEC") || Parameter == ("-dnssec"))
			{
				EDNS0 = true;
				DNSSEC = true;
			}
		//Specifie Query Type.
			else if (Parameter == ("-QT") || Parameter == ("-qt"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

				//Type name
					if (Parameter == ("A") || Parameter == ("a"))
					{
						QueryParameter.Type = htons(DNS_A_RECORDS);
					}
					else if (Parameter == ("NS") || Parameter == ("ns"))
					{
						QueryParameter.Type = htons(DNS_NS_RECORDS);
					}
					else if (Parameter == ("CNAME") || Parameter == ("cname"))
					{
						QueryParameter.Type = htons(DNS_CNAME_RECORDS);
					}
					else if (Parameter == ("SOA") || Parameter == ("soa"))
					{
						QueryParameter.Type = htons(DNS_SOA_RECORDS);
					}
					else if (Parameter == ("PTR") || Parameter == ("ptr"))
					{
						QueryParameter.Type = htons(DNS_PTR_RECORDS);
					}
					else if (Parameter == ("MX") || Parameter == ("mx"))
					{
						QueryParameter.Type = htons(DNS_MX_RECORDS);
					}
					else if (Parameter == ("TXT") || Parameter == ("txt"))
					{
						QueryParameter.Type = htons(DNS_TXT_RECORDS);
					}
					else if (Parameter == ("RP") || Parameter == ("rp"))
					{
						QueryParameter.Type = htons(DNS_RP_RECORDS);
					}
					else if (Parameter == ("SIG") || Parameter == ("sig"))
					{
						QueryParameter.Type = htons(DNS_SIG_RECORDS);
					}
					else if (Parameter == ("KEY") || Parameter == ("key"))
					{
						QueryParameter.Type = htons(DNS_KEY_RECORDS);
					}
					else if (Parameter == ("AAAA") || Parameter == ("aaaa"))
					{
						QueryParameter.Type = htons(DNS_AAAA_RECORDS);
					}
					else if (Parameter == ("LOC") || Parameter == ("loc"))
					{
						QueryParameter.Type = htons(DNS_LOC_RECORDS);
					}
					else if (Parameter == ("SRV") || Parameter == ("srv"))
					{
						QueryParameter.Type = htons(DNS_SRV_RECORDS);
					}
					else if (Parameter == ("NAPTR") || Parameter == ("naptr"))
					{
						QueryParameter.Type = htons(DNS_NAPTR_RECORDS);
					}
					else if (Parameter == ("KX") || Parameter == ("kx"))
					{
						QueryParameter.Type = htons(DNS_KX_RECORDS);
					}
					else if (Parameter == ("CERT") || Parameter == ("cert"))
					{
						QueryParameter.Type = htons(DNS_CERT_RECORDS);
					}
					else if (Parameter == ("DNAME") || Parameter == ("dname"))
					{
						QueryParameter.Type = htons(DNS_DNAME_RECORDS);
					}
					else if (Parameter == ("EDNS0") || Parameter == ("edns0"))
					{
						QueryParameter.Type = htons(DNS_EDNS0_RECORDS);
					}
					else if (Parameter == ("APL") || Parameter == ("apl"))
					{
						QueryParameter.Type = htons(DNS_APL_RECORDS);
					}
					else if (Parameter == ("DS") || Parameter == ("ds"))
					{
						QueryParameter.Type = htons(DNS_DS_RECORDS);
					}
					else if (Parameter == ("SSHFP") || Parameter == ("sshfp"))
					{
						QueryParameter.Type = htons(DNS_SSHFP_RECORDS);
					}
					else if (Parameter == ("IPSECKEY") || Parameter == ("ipseckey"))
					{
						QueryParameter.Type = htons(DNS_IPSECKEY_RECORDS);
					}
					else if (Parameter == ("RRSIG") || Parameter == ("rrsig"))
					{
						QueryParameter.Type = htons(DNS_RRSIG_RECORDS);
					}
					else if (Parameter == ("NSEC") || Parameter == ("nsec"))
					{
						QueryParameter.Type = htons(DNS_NSEC_RECORDS);
					}
					else if (Parameter == ("DNSKEY") || Parameter == ("dnskey"))
					{
						QueryParameter.Type = htons(DNS_DNSKEY_RECORDS);
					}
					else if (Parameter == ("DHCID") || Parameter == ("dhcid"))
					{
						QueryParameter.Type = htons(DNS_DHCID_RECORDS);
					}
					else if (Parameter == ("NSEC3") || Parameter == ("nsec3"))
					{
						QueryParameter.Type = htons(DNS_NSEC3_RECORDS);
					}
					else if (Parameter == ("NSEC3PARAM") || Parameter == ("nsec3param"))
					{
						QueryParameter.Type = htons(DNS_NSEC3PARAM_RECORDS);
					}
					else if (Parameter == ("HIP") || Parameter == ("hip"))
					{
						QueryParameter.Type = htons(DNS_HIP_RECORDS);
					}
					else if (Parameter == ("SPF") || Parameter == ("spf"))
					{
						QueryParameter.Type = htons(DNS_SPF_RECORDS);
					}
					else if (Parameter == ("TKEY") || Parameter == ("tkey"))
					{
						QueryParameter.Type = htons(DNS_TKEY_RECORDS);
					}
					else if (Parameter == ("TSIG") || Parameter == ("tsig"))
					{
						QueryParameter.Type = htons(DNS_TSIG_RECORDS);
					}
					else if (Parameter == ("IXFR") || Parameter == ("ixfr"))
					{
						QueryParameter.Type = htons(DNS_IXFR_RECORDS);
					}
					else if (Parameter == ("AXFR") || Parameter == ("axfr"))
					{
						QueryParameter.Type = htons(DNS_AXFR_RECORDS);
					}
					else if (Parameter == ("ANY") || Parameter == ("any"))
					{
						QueryParameter.Type = htons(DNS_ANY_RECORDS);
					}
					else if (Parameter == ("TA") || Parameter == ("ta"))
					{
						QueryParameter.Type = htons(DNS_TA_RECORDS);
					}
					else if (Parameter == ("DLV") || Parameter == ("dlv"))
					{
						QueryParameter.Type = htons(DNS_DLV_RECORDS);
					}
				//Type number
					else {
						Result = strtol(Parameter.c_str(), nullptr, NULL);
						if (Result > 0 && Result <= U16_MAXNUM)
						{
							QueryParameter.Type = htons((uint16_t)Result);
						}
						else {
							wprintf(L"\nParameter[-qt Type] error.\n");

							WSACleanup();
							return EXIT_FAILURE;
						}
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Specifie Query Classes.
			else if (Parameter == ("-QC") || Parameter == ("-qc"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

				//Classes name
					if (Parameter == ("INTERNET") || Parameter == ("internet") || Parameter == ("IN") || Parameter == ("in"))
					{
						QueryParameter.Classes = htons(DNS_CLASS_IN);
					}
					else if (Parameter == ("CSNET") || Parameter == ("csnet"))
					{
						QueryParameter.Classes = htons(DNS_CLASS_CSNET);
					}
					else if (Parameter == ("CHAOS") || Parameter == ("chaos"))
					{
						QueryParameter.Classes = htons(DNS_CLASS_CHAOS);
					}
					else if (Parameter == ("HESIOD") || Parameter == ("hesiod"))
					{
						QueryParameter.Classes = htons(DNS_CLASS_HESIOD);
					}
					else if (Parameter == ("NONE") || Parameter == ("none"))
					{
						QueryParameter.Classes = htons(DNS_CLASS_NONE);
					}
					else if (Parameter == ("ALL") || Parameter == ("ANY") || Parameter == ("all") || Parameter == ("any"))
					{
						QueryParameter.Classes = htons(DNS_CLASS_ANY);
					}
				//Classes number
					else {
						Result = strtol(Parameter.c_str(), nullptr, NULL);
						if (Result > 0 && Result <= U16_MAXNUM)
						{
							QueryParameter.Classes = htons((uint16_t)Result);
						}
						else {
							wprintf(L"\nParameter[-qc Classes] error.\n");

							WSACleanup();
							return EXIT_FAILURE;
						}
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Specifie requesting server name or port.
			else if (Parameter == ("-p"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

				//Server name
					if (Parameter == ("TCPMUX") || Parameter == ("tcpmux"))
					{
						ServiceName = htons(IPPORT_TCPMUX);
					}
					else if (Parameter == ("ECHO") || Parameter == ("echo"))
					{
						ServiceName = htons(IPPORT_ECHO);
					}
					else if (Parameter == ("DISCARD") || Parameter == ("discard"))
					{
						ServiceName = htons(IPPORT_DISCARD);
					}
					else if (Parameter == ("SYSTAT") || Parameter == ("systat"))
					{
						ServiceName = htons(IPPORT_SYSTAT);
					}
					else if (Parameter == ("DAYTIME") || Parameter == ("daytime"))
					{
						ServiceName = htons(IPPORT_DAYTIME);
					}
					else if (Parameter == ("NETSTAT") || Parameter == ("netstat"))
					{
						ServiceName = htons(IPPORT_NETSTAT);
					}
					else if (Parameter == ("QOTD") || Parameter == ("qotd"))
					{
						ServiceName = htons(IPPORT_QOTD);
					}
					else if (Parameter == ("MSP") || Parameter == ("msp"))
					{
						ServiceName = htons(IPPORT_MSP);
					}
					else if (Parameter == ("CHARGEN") || Parameter == ("chargen"))
					{
						ServiceName = htons(IPPORT_CHARGEN);
					}
					else if (Parameter == ("FTPDATA") || Parameter == ("ftpdata"))
					{
						ServiceName = htons(IPPORT_FTP_DATA);
					}
					else if (Parameter == ("FTP") || Parameter == ("ftp"))
					{
						ServiceName = htons(IPPORT_FTP);
					}
					else if (Parameter == ("SSH") || Parameter == ("ssh"))
					{
						ServiceName = htons(IPPORT_SSH);
					}
					else if (Parameter == ("TELNET") || Parameter == ("telnet"))
					{
						ServiceName = htons(IPPORT_TELNET);
					}
					else if (Parameter == ("SMTP") || Parameter == ("smtp"))
					{
						ServiceName = htons(IPPORT_SMTP);
					}
					else if (Parameter == ("TIME") || Parameter == ("time"))
					{
						ServiceName = htons(IPPORT_TIMESERVER);
					}
					else if (Parameter == ("RAP") || Parameter == ("rap"))
					{
						ServiceName = htons(IPPORT_RAP);
					}
					else if (Parameter == ("RLP") || Parameter == ("rlp"))
					{
						ServiceName = htons(IPPORT_RLP);
					}
					else if (Parameter == ("NAME") || Parameter == ("name"))
					{
						ServiceName = htons(IPPORT_NAMESERVER);
					}
					else if (Parameter == ("WHOIS") || Parameter == ("whois"))
					{
						ServiceName = htons(IPPORT_WHOIS);
					}
					else if (Parameter == ("TACACS") || Parameter == ("tacacs"))
					{
						ServiceName = htons(IPPORT_TACACS);
					}
					else if (Parameter == ("XNSAUTH") || Parameter == ("xnsauth"))
					{
						ServiceName = htons(IPPORT_XNSAUTH);
					}
					else if (Parameter == ("MTP") || Parameter == ("mtp"))
					{
						ServiceName = htons(IPPORT_MTP);
					}
					else if (Parameter == ("BOOTPS") || Parameter == ("bootps"))
					{
						ServiceName = htons(IPPORT_BOOTPS);
					}
					else if (Parameter == ("BOOTPC") || Parameter == ("bootpc"))
					{
						ServiceName = htons(IPPORT_BOOTPC);
					}
					else if (Parameter == ("TFTP") || Parameter == ("tftp"))
					{
						ServiceName = htons(IPPORT_TFTP);
					}
					else if (Parameter == ("RJE") || Parameter == ("rje"))
					{
						ServiceName = htons(IPPORT_RJE);
					}
					else if (Parameter == ("FINGER") || Parameter == ("finger"))
					{
						ServiceName = htons(IPPORT_FINGER);
					}
					else if (Parameter == ("HTTP") || Parameter == ("http"))
					{
						ServiceName = htons(IPPORT_HTTP);
					}
					else if (Parameter == ("HTTPBACKUP") || Parameter == ("httpbackup"))
					{
						ServiceName = htons(IPPORT_HTTPBACKUP);
					}
					else if (Parameter == ("TTYLINK") || Parameter == ("ttylink"))
					{
						ServiceName = htons(IPPORT_TTYLINK);
					}
					else if (Parameter == ("SUPDUP") || Parameter == ("supdup"))
					{
						ServiceName = htons(IPPORT_SUPDUP);
					}
					else if (Parameter == ("POP3") || Parameter == ("pop3"))
					{
						ServiceName = htons(IPPORT_POP3);
					}
					else if (Parameter == ("SUNRPC") || Parameter == ("sunrpc"))
					{
						ServiceName = htons(IPPORT_SUNRPC);
					}
					else if (Parameter == ("SQL") || Parameter == ("sql"))
					{
						ServiceName = htons(IPPORT_SQL);
					}
					else if (Parameter == ("NTP") || Parameter == ("ntp"))
					{
						ServiceName = htons(IPPORT_NTP);
					}
					else if (Parameter == ("EPMAP") || Parameter == ("epmap"))
					{
						ServiceName = htons(IPPORT_EPMAP);
					}
					else if (Parameter == ("NETBIOSNS") || Parameter == ("netbiosns"))
					{
						ServiceName = htons(IPPORT_NETBIOS_NS);
					}
					else if (Parameter == ("NETBIOSDGM") || Parameter == ("netbiosdgm"))
					{
						ServiceName = htons(IPPORT_NETBIOS_DGM);
					}
					else if (Parameter == ("NETBIOSSSN") || Parameter == ("netbiosssn"))
					{
						ServiceName = htons(IPPORT_NETBIOS_SSN);
					}
					else if (Parameter == ("IMAP") || Parameter == ("imap"))
					{
						ServiceName = htons(IPPORT_IMAP);
					}
					else if (Parameter == ("BFTP") || Parameter == ("bftp"))
					{
						ServiceName = htons(IPPORT_BFTP);
					}
					else if (Parameter == ("SGMP") || Parameter == ("sgmp"))
					{
						ServiceName = htons(IPPORT_SGMP);
					}
					else if (Parameter == ("SQLSRV") || Parameter == ("sqlsrv"))
					{
						ServiceName = htons(IPPORT_SQLSRV);
					}
					else if (Parameter == ("DMSP") || Parameter == ("dmsp"))
					{
						ServiceName = htons(IPPORT_DMSP);
					}
					else if (Parameter == ("SNMP") || Parameter == ("snmp"))
					{
						ServiceName = htons(IPPORT_SNMP);
					}
					else if (Parameter == ("SNMPTRAP") || Parameter == ("snmptrap"))
					{
						ServiceName = htons(IPPORT_SNMP_TRAP);
					}
					else if (Parameter == ("ATRTMP") || Parameter == ("atrtmp"))
					{
						ServiceName = htons(IPPORT_ATRTMP);
					}
					else if (Parameter == ("ATHBP") || Parameter == ("athbp"))
					{
						ServiceName = htons(IPPORT_ATHBP);
					}
					else if (Parameter == ("QMTP") || Parameter == ("qmtp"))
					{
						ServiceName = htons(IPPORT_QMTP);
					}
					else if (Parameter == ("IPX") || Parameter == ("ipx"))
					{
						ServiceName = htons(IPPORT_IPX);
					}
					else if (Parameter == ("IMAP3") || Parameter == ("imap3"))
					{
						ServiceName = htons(IPPORT_IMAP3);
					}
					else if (Parameter == ("BGMP") || Parameter == ("bgmp"))
					{
						ServiceName = htons(IPPORT_BGMP);
					}
					else if (Parameter == ("TSP") || Parameter == ("tsp"))
					{
						ServiceName = htons(IPPORT_TSP);
					}
					else if (Parameter == ("IMMP") || Parameter == ("immp"))
					{
						ServiceName = htons(IPPORT_IMMP);
					}
					else if (Parameter == ("ODMR") || Parameter == ("odmr"))
					{
						ServiceName = htons(IPPORT_ODMR);
					}
					else if (Parameter == ("RPC2PORTMAP") || Parameter == ("rpc2portmap"))
					{
						ServiceName = htons(IPPORT_RPC2PORTMAP);
					}
					else if (Parameter == ("CLEARCASE") || Parameter == ("clearcase"))
					{
						ServiceName = htons(IPPORT_CLEARCASE);
					}
					else if (Parameter == ("HPALARMMGR") || Parameter == ("hpalarmmgr"))
					{
						ServiceName = htons(IPPORT_HPALARMMGR);
					}
					else if (Parameter == ("ARNS") || Parameter == ("arns"))
					{
						ServiceName = htons(IPPORT_ARNS);
					}
					else if (Parameter == ("AURP") || Parameter == ("aurp"))
					{
						ServiceName = htons(IPPORT_AURP);
					}
					else if (Parameter == ("LDAP") || Parameter == ("ldap"))
					{
						ServiceName = htons(IPPORT_LDAP);
					}
					else if (Parameter == ("UPS") || Parameter == ("ups"))
					{
						ServiceName = htons(IPPORT_UPS);
					}
					else if (Parameter == ("SLP") || Parameter == ("slp"))
					{
						ServiceName = htons(IPPORT_SLP);
					}
					else if (Parameter == ("HTTPS") || Parameter == ("https"))
					{
						ServiceName = htons(IPPORT_HTTPS);
					}
					else if (Parameter == ("SNPP") || Parameter == ("snpp"))
					{
						ServiceName = htons(IPPORT_SNPP);
					}
					else if (Parameter == ("MICROSOFTDS") || Parameter == ("microsoftds"))
					{
						ServiceName = htons(IPPORT_MICROSOFT_DS);
					}
					else if (Parameter == ("KPASSWD") || Parameter == ("kpasswd"))
					{
						ServiceName = htons(IPPORT_KPASSWD);
					}
					else if (Parameter == ("TCPNETHASPSRV") || Parameter == ("tcpnethaspsrv"))
					{
						ServiceName = htons(IPPORT_TCPNETHASPSRV);
					}
					else if (Parameter == ("RETROSPECT") || Parameter == ("retrospect"))
					{
						ServiceName = htons(IPPORT_RETROSPECT);
					}
					else if (Parameter == ("ISAKMP") || Parameter == ("isakmp"))
					{
						ServiceName = htons(IPPORT_ISAKMP);
					}
					else if (Parameter == ("BIFFUDP") || Parameter == ("biffudp"))
					{
						ServiceName = htons(IPPORT_BIFFUDP);
					}
					else if (Parameter == ("WHOSERVER") || Parameter == ("whoserver"))
					{
						ServiceName = htons(IPPORT_WHOSERVER);
					}
					else if (Parameter == ("SYSLOG") || Parameter == ("syslog"))
					{
						ServiceName = htons(IPPORT_SYSLOG);
					}
					else if (Parameter == ("ROUTERSERVER") || Parameter == ("routerserver"))
					{
						ServiceName = htons(IPPORT_ROUTESERVER);
					}
					else if (Parameter == ("NCP") || Parameter == ("ncp"))
					{
						ServiceName = htons(IPPORT_NCP);
					}
					else if (Parameter == ("COURIER") || Parameter == ("courier"))
					{
						ServiceName = htons(IPPORT_COURIER);
					}
					else if (Parameter == ("COMMERCE") || Parameter == ("commerce"))
					{
						ServiceName = htons(IPPORT_COMMERCE);
					}
					else if (Parameter == ("RTSP") || Parameter == ("rtsp"))
					{
						ServiceName = htons(IPPORT_RTSP);
					}
					else if (Parameter == ("NNTP") || Parameter == ("nntp"))
					{
						ServiceName = htons(IPPORT_NNTP);
					}
					else if (Parameter == ("HTTPRPCEPMAP") || Parameter == ("httprpcepmap"))
					{
						ServiceName = htons(IPPORT_HTTPRPCEPMAP);
					}
					else if (Parameter == ("IPP") || Parameter == ("ipp"))
					{
						ServiceName = htons(IPPORT_IPP);
					}
					else if (Parameter == ("LDAPS") || Parameter == ("ldaps"))
					{
						ServiceName = htons(IPPORT_LDAPS);
					}
					else if (Parameter == ("MSDP") || Parameter == ("msdp"))
					{
						ServiceName = htons(IPPORT_MSDP);
					}
					else if (Parameter == ("AODV") || Parameter == ("aodv"))
					{
						ServiceName = htons(IPPORT_AODV);
					}
					else if (Parameter == ("FTPSDATA") || Parameter == ("ftpsdata"))
					{
						ServiceName = htons(IPPORT_FTPSDATA);
					}
					else if (Parameter == ("FTPS") || Parameter == ("ftps"))
					{
						ServiceName = htons(IPPORT_FTPS);
					}
					else if (Parameter == ("NAS") || Parameter == ("nas"))
					{
						ServiceName = htons(IPPORT_NAS);
					}
					else {
						if (Parameter == ("TELNETS") || Parameter == ("telnets"))
							ServiceName = htons(IPPORT_TELNETS);

					//Number port
						Result = strtol(Parameter.c_str(), nullptr, NULL);
						if (Result > 0 && Result <= U16_MAXNUM)
						{
							ServiceName = htons((uint16_t)Result);
						}
						else {
							wprintf(L"\nParameter[-p ServiceName/Protocol] error.\n");

							WSACleanup();
							return EXIT_FAILURE;
						}
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Specifie Raw data to send.
			else if (Parameter == ("-RAWDATA") || Parameter == ("-rawdata"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;

				//Initialization
					std::string RawDataString = argv[Index];
					if (RawDataString.length() < PACKET_MINSIZE && RawDataString.length() > PACKET_MAXSIZE)
					{
						wprintf(L"\nParameter[-rawdata RAW_Data] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
					std::shared_ptr<char> TempRawData(new char[PACKET_MAXSIZE]());
					RawData.swap(TempRawData);
					TempRawData.reset();
					std::shared_ptr<char> Temp(new char[5U]());
					Temp.get()[0] = 48; //"0"
					Temp.get()[1U] = 120; //"x"

				//Read raw data.
					for (size_t InnerIndex = 0;InnerIndex < RawDataString.length();InnerIndex++)
					{
						Temp.get()[2U] = RawDataString[InnerIndex];
						InnerIndex++;
						Temp.get()[3U] = RawDataString[InnerIndex];
						Result = strtol(Temp.get(), nullptr, NULL);
						if (Result > 0 && Result <= U8_MAXNUM)
						{
							RawData.get()[RawDataLen] = (char)Result;
							RawDataLen++;
						}
						else {
							wprintf(L"\nParameter[-rawdata RAW_Data] error.\n");

							WSACleanup();
							return EXIT_FAILURE;
						}
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Send RAW data with Raw Socket.
			else if (Parameter == ("-RAW") || Parameter == ("-raw"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

				//Protocol name
					RawSocket = true;
					if (Parameter == ("UDP") || Parameter == ("udp"))
					{
						RawSocket = false;
						continue;
					}
					else if (Parameter == ("HOPOPTS") || Parameter == ("hopopts"))
					{
						ServiceName = IPPROTO_HOPOPTS;
					}
					else if (Parameter == ("ICMP") || Parameter == ("icmp"))
					{
						ServiceName = IPPROTO_ICMP;
					}
					else if (Parameter == ("IGMP") || Parameter == ("igmp"))
					{
						ServiceName = IPPROTO_IGMP;
					}
					else if (Parameter == ("GGP") || Parameter == ("ggp"))
					{
						ServiceName = IPPROTO_GGP;
					}
					else if (Parameter == ("IPV4") || Parameter == ("ipv4"))
					{
						ServiceName = IPPROTO_IPV4;
					}
					else if (Parameter == ("ST") || Parameter == ("st"))
					{
						ServiceName = IPPROTO_ST;
					}
					else if (Parameter == ("TCP") || Parameter == ("tcp"))
					{
						ServiceName = IPPROTO_TCP;
					}
					else if (Parameter == ("CBT") || Parameter == ("cbt"))
					{
						ServiceName = IPPROTO_CBT;
					}
					else if (Parameter == ("EGP") || Parameter == ("egp"))
					{
						ServiceName = IPPROTO_EGP;
					}
					else if (Parameter == ("IGP") || Parameter == ("igp"))
					{
						ServiceName = IPPROTO_IGP;
					}
					else if (Parameter == ("PUP") || Parameter == ("pup"))
					{
						ServiceName = IPPROTO_PUP;
					}
					else if (Parameter == ("IDP") || Parameter == ("idp"))
					{
						ServiceName = IPPROTO_IDP;
					}
					else if (Parameter == ("IPV6") || Parameter == ("ipv6"))
					{
						ServiceName = IPPROTO_IPV6;
					}
					else if (Parameter == ("ROUTING") || Parameter == ("routing"))
					{
						ServiceName = IPPROTO_ROUTING;
					}
					else if (Parameter == ("ESP") || Parameter == ("esp"))
					{
						ServiceName = IPPROTO_ESP;
					}
					else if (Parameter == ("FRAGMENT") || Parameter == ("fragment"))
					{
						ServiceName = IPPROTO_FRAGMENT;
					}
					else if (Parameter == ("AH") || Parameter == ("ah"))
					{
						ServiceName = IPPROTO_AH;
					}
					else if (Parameter == ("ICMPV6") || Parameter == ("icmpv6"))
					{
						ServiceName = IPPROTO_ICMPV6;
					}
					else if (Parameter == ("NONE") || Parameter == ("none"))
					{
						ServiceName = IPPROTO_NONE;
					}
					else if (Parameter == ("DSTOPTS") || Parameter == ("dstopts"))
					{
						ServiceName = IPPROTO_DSTOPTS;
					}
					else if (Parameter == ("ND") || Parameter == ("nd"))
					{
						ServiceName = IPPROTO_ND;
					}
					else if (Parameter == ("ICLFXBM") || Parameter == ("iclfxbm"))
					{
						ServiceName = IPPROTO_ICLFXBM;
					}
					else if (Parameter == ("PIM") || Parameter == ("pim"))
					{
						ServiceName = IPPROTO_PIM;
					}
					else if (Parameter == ("PGM") || Parameter == ("pgm"))
					{
						ServiceName = IPPROTO_PGM;
					}
					else if (Parameter == ("L2TP") || Parameter == ("l2tp"))
					{
						ServiceName = IPPROTO_L2TP;
					}
					else if (Parameter == ("SCTP") || Parameter == ("sctp"))
					{
						ServiceName = IPPROTO_SCTP;
					}
					else if (Parameter == ("RAW") || Parameter == ("raw"))
					{
						ServiceName = IPPROTO_RAW;
					}
				//Protocol number
					else {
						Result = strtol(Parameter.c_str(), nullptr, NULL);
						if (Result > 0 && Result <= U4_MAXNUM)
						{
							ServiceName = (uint8_t)Result;
						}
						else {
							wprintf(L"\nParameter[-raw ServiceName] error.\n");

							WSACleanup();
							return EXIT_FAILURE;
						}
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Specifie buffer size.
			else if (Parameter == ("-Buf") || Parameter == ("-buf"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					Result = strtol(Parameter.c_str(), nullptr, NULL);
					if (Result >= OLD_DNS_MAXSIZE && Result <= LARGE_PACKET_MAXSIZE)
					{
						BufferSize = Result;
					}
					else {
						wprintf(L"\nParameter[-buf Size] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Output result to file.
			else if (Parameter == ("-OF") || Parameter == ("-of"))
			{
				if (Index + 1U < (size_t)argc)
				{
					Index++;
					Parameter = argv[Index];

					if (Parameter.length() <= MAX_PATH)
					{
						std::shared_ptr<wchar_t> OutputFileNamePTR(new wchar_t[MAX_PATH]());
						MultiByteToWideChar(CP_ACP, NULL, Parameter.c_str(), MBSTOWCS_NULLTERMINATE, OutputFileNamePTR.get(), (int)Parameter.length());
						OutputFileName = OutputFileNamePTR.get();
					}
					else {
						wprintf(L"\nParameter[-of FileName] error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nNot enough parameters error.\n");

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		//Using IPv6.
			else if (Parameter == ("-6"))
			{
				Protocol = AF_INET6;
			}
		//Using IPv4.
			else if (Parameter == ("-4"))
			{
				Protocol = AF_INET;
			}
		//Specifie Query Domain.
			else if (!RawData && TestDomain.empty() && Index == argc - 2 && Parameter.length() > 2U)
			{
				TestDomain = Parameter;
			}
		//Specifie target.
			else if (Index == argc - 1 && Parameter.length() > 2U)
			{
			//IPv6 address
				if (Parameter.find(58) != std::string::npos)
				{
					if (Protocol == AF_INET)
					{
						wprintf(L"\nTarget protocol error.\n");

						WSACleanup();
						return EXIT_FAILURE;
					}

					Protocol = AF_INET6;
					SockAddr.ss_family = AF_INET6;
					if (AddressStringToBinary((PSTR)Parameter.c_str(), &((PSOCKADDR_IN6)&SockAddr)->sin6_addr, AF_INET6, Result) == EXIT_FAILURE)
					{
						wprintf(L"\nTarget format error, error code is %d.\n", (int)Result);

						WSACleanup();
						return EXIT_FAILURE;
					}

					TargetString.append("[");
					TargetString.append(Parameter);
					TargetString.append("]");
				}
				else {
					for (auto StringIter = Parameter.begin();StringIter != Parameter.end();StringIter++)
					{
					//Domain
						if (*StringIter < 46 || *StringIter == 47 || *StringIter > 57)
						{
							ADDRINFOA AddrInfoHints = {0}, *AddrInfo = nullptr;
						//Try with IPv6.
							if (Protocol == 0)
								Protocol = AF_INET6;
							AddrInfoHints.ai_family = Protocol;
							SockAddr.ss_family = Protocol;

						//Get address.
							if (getaddrinfo(Parameter.c_str(), NULL, &AddrInfoHints, &AddrInfo) != 0)
							{
							//Retry with IPv4.
								Protocol = AF_INET;
								AddrInfoHints.ai_family = Protocol;
								SockAddr.ss_family = Protocol;

								if (getaddrinfo(Parameter.c_str(), NULL, &AddrInfoHints, &AddrInfo) != 0)
								{
									wprintf(L"\nResolve domain name error, error code is %d.\n", WSAGetLastError());

									WSACleanup();
									return EXIT_FAILURE;
								}
							}

						//Get address form PTR.
							if (AddrInfo != nullptr)
							{
								for (auto PTR = AddrInfo;PTR != nullptr;PTR = PTR->ai_next)
								{
								//IPv6
									if (PTR->ai_family == AF_INET6 && SockAddr.ss_family == AF_INET6 && 
										!IN6_IS_ADDR_LINKLOCAL((in6_addr *)(PTR->ai_addr)) &&
										!(((PSOCKADDR_IN6)(PTR->ai_addr))->sin6_scope_id == 0)) //Get port from first(Main) IPv6 device
									{
										((PSOCKADDR_IN6)&SockAddr)->sin6_addr = ((PSOCKADDR_IN6)(PTR->ai_addr))->sin6_addr;

									//Get string of address.
										TargetDomainString = Parameter;
										std::shared_ptr<char> Buffer(new char[ADDR_STRING_MAXSIZE]());

									//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
									#ifdef _WIN64
										inet_ntop(AF_INET6, &((PSOCKADDR_IN6)&SockAddr)->sin6_addr, Buffer.get(), ADDR_STRING_MAXSIZE);
									#else //x86
										DWORD BufferLength = ADDR_STRING_MAXSIZE;
										WSAAddressToStringA((LPSOCKADDR)&SockAddr, sizeof(sockaddr_in6), NULL, Buffer.get(), &BufferLength);
									#endif

										TargetString.append("[");
										TargetString.append(Buffer.get());
										TargetString.append("]");
										break;
									}
								//IPv4
									else if (PTR->ai_family == AF_INET && SockAddr.ss_family == AF_INET && 
										((PSOCKADDR_IN)(PTR->ai_addr))->sin_addr.S_un.S_addr != INADDR_LOOPBACK && 
										((PSOCKADDR_IN)(PTR->ai_addr))->sin_addr.S_un.S_addr != INADDR_BROADCAST)
									{
										((PSOCKADDR_IN)&SockAddr)->sin_addr = ((PSOCKADDR_IN)(PTR->ai_addr))->sin_addr;

									//Get string of address.
										TargetDomainString = Parameter;
										std::shared_ptr<char> Buffer(new char[ADDR_STRING_MAXSIZE]());

									//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
									#ifdef _WIN64
										inet_ntop(AF_INET, &((PSOCKADDR_IN)&SockAddr)->sin_addr, Buffer.get(), ADDR_STRING_MAXSIZE);
									#else //x86
										DWORD BufferLength = ADDR_STRING_MAXSIZE;
										WSAAddressToStringA((LPSOCKADDR)&SockAddr, sizeof(sockaddr_in), NULL, Buffer.get(), &BufferLength);
									#endif

										TargetString = Buffer.get();
										break;
									}
								}

								freeaddrinfo(AddrInfo);
							}

							break;
						}

					//IPv4
						if (StringIter == Parameter.end() - 1U)
						{
							if (Protocol == AF_INET6)
							{
								wprintf(L"\nTarget protocol error.\n");

								WSACleanup();
								return EXIT_FAILURE;
							}

							Protocol = AF_INET;
							SockAddr.ss_family = AF_INET;
							if (AddressStringToBinary((PSTR)Parameter.c_str(), &((PSOCKADDR_IN)&SockAddr)->sin_addr, AF_INET, Result) == EXIT_FAILURE)
							{
								wprintf(L"\nTarget format error, error code is %d.\n", (int)Result);

								WSACleanup();
								return EXIT_FAILURE;
							}

							TargetString = Parameter;
						}
					}
				}
			}
		}

	//Check parameter reading.
		if (SockAddr.ss_family == AF_INET6)
		{
			if (CheckEmptyBuffer(&((PSOCKADDR_IN6)&SockAddr)->sin6_addr, sizeof(in6_addr)))
			{
				wprintf(L"\nTarget is empty.\n");

				WSACleanup();
				return EXIT_FAILURE;
			}
			else {
			//Mark port.
				if (ServiceName == 0)
				{
					ServiceName = htons(DNS_PORT);
					((PSOCKADDR_IN6)&SockAddr)->sin6_port = htons(DNS_PORT);
				}
				else {
					((PSOCKADDR_IN6)&SockAddr)->sin6_port = ServiceName;
				}
			}
		}
		else { //IPv4
			if (((PSOCKADDR_IN)&SockAddr)->sin_addr.S_un.S_addr == 0)
			{
				wprintf(L"\nTarget is empty.\n");

				WSACleanup();
				return EXIT_FAILURE;
			}
			else {
			//Mark port.
				if (ServiceName == 0)
				{
					ServiceName = htons(DNS_PORT);
					((PSOCKADDR_IN)&SockAddr)->sin_port = htons(DNS_PORT);
				}
				else {
					((PSOCKADDR_IN)&SockAddr)->sin_port = ServiceName;
				}
			}
		}

	//Check parameter.
		if (SocketTimeout == DEFAULT_TIME_OUT && !IsHigherThanWin7())
			SocketTimeout = DEFAULT_TIME_OUT - 500U;
		MinTime = SocketTimeout;

	//Convert Multi byte(s) to wide char(s).
		std::wstring wTestDomain, wTargetDomainString;
		std::shared_ptr<wchar_t> wTargetStringPTR(new wchar_t[LARGE_PACKET_MAXSIZE]());
		if (TargetString.length() > LARGE_PACKET_MAXSIZE || TargetDomainString.length() > LARGE_PACKET_MAXSIZE || TestDomain.length() > LARGE_PACKET_MAXSIZE)
		{
			wprintf(L"\nTest Domain or Target is/are too long.\n");
			return EXIT_FAILURE;
		}
		MultiByteToWideChar(CP_ACP, NULL, TargetString.c_str(), MBSTOWCS_NULLTERMINATE, wTargetStringPTR.get(), (int)TargetString.length());
		wTargetString = wTargetStringPTR.get();
		memset(wTargetStringPTR.get(), 0, sizeof(wchar_t) * LARGE_PACKET_MAXSIZE);
		MultiByteToWideChar(CP_ACP, NULL, TestDomain.c_str(), MBSTOWCS_NULLTERMINATE, wTargetStringPTR.get(), (int)TestDomain.length());
		wTestDomain = wTargetStringPTR.get();
		if (!TargetDomainString.empty())
		{
			memset(wTargetStringPTR.get(), 0, sizeof(wchar_t) * LARGE_PACKET_MAXSIZE);
			MultiByteToWideChar(CP_ACP, NULL, TargetDomainString.c_str(), MBSTOWCS_NULLTERMINATE, wTargetStringPTR.get(), (int)TargetDomainString.length());
			wTargetDomainString = wTargetStringPTR.get();
		}
		wTargetStringPTR.reset();

	//Check DNS header.
		if (HeaderParameter.Flags == 0)
			HeaderParameter.Flags = htons(DNS_STANDARD);
		if (HeaderParameter.Questions == 0)
			HeaderParameter.Questions = htons(0x0001);

	//Check DNS query.
		if (QueryParameter.Classes == 0)
			QueryParameter.Classes = htons(DNS_CLASS_IN);
		if (QueryParameter.Type == 0)
		{
			if (SockAddr.ss_family == AF_INET6) //IPv6
				QueryParameter.Type = htons(DNS_AAAA_RECORDS);
			else //IPv4
				QueryParameter.Type = htons(DNS_A_RECORDS);
		}

	//Check EDNS0 Label.
		if (DNSSEC)
			EDNS0 = true;
		if (EDNS0)
		{
			HeaderParameter.Additional = htons(0x0001);
			EDNS0Parameter.Type = htons(DNS_EDNS0_RECORDS);
			if (EDNS0PayloadSize == 0)
				EDNS0Parameter.UDPPayloadSize = htons(EDNS0_MINSIZE);
			else 
				EDNS0Parameter.UDPPayloadSize = htons((uint16_t)EDNS0PayloadSize);
			if (DNSSEC)
			{
				HeaderParameter.FlagsBits.AD = ~HeaderParameter.FlagsBits.AD; //Local DNSSEC Server validate
				HeaderParameter.FlagsBits.CD = ~HeaderParameter.FlagsBits.CD; //Client validate
				EDNS0Parameter.Z_Bits.DO = ~EDNS0Parameter.Z_Bits.DO; //Accepts DNSSEC security RRs
			}
		}

	//Output result to file.
		if (!OutputFileName.empty())
		{
			Result = _wfopen_s(&OutputFile, OutputFileName.c_str(), L"w,ccs=UTF-8");
			if (OutputFile == nullptr)
			{
				wprintf(L"Create output result file %ls error, error code is %d.\n", OutputFileName.c_str(), (int)Result);

				WSACleanup();
				return EXIT_SUCCESS;
			}
			else {
				fwprintf_s(OutputFile, L"\n");
			}
		}

	//Print to screen before sending.
		wprintf(L"\n");
		if (ReverseLookup)
		{
			if (wTargetDomainString.empty())
			{
				std::shared_ptr<char> FQDN(new char[NI_MAXHOST]());
				if (getnameinfo((PSOCKADDR)&SockAddr, sizeof(sockaddr_in), FQDN.get(), NI_MAXHOST, NULL, NULL, NI_NUMERICSERV) != 0)
				{
					wprintf(L"\nResolve addresses to host names error, error code is %d.\n", WSAGetLastError());
					wprintf(L"DNSPing %ls:%u with %ls:\n", wTargetString.c_str(), ntohs(ServiceName), wTestDomain.c_str());

				//Output to file.
					if (OutputFile != nullptr)
						fwprintf_s(OutputFile, L"DNSPing %ls:%u with %ls:\n", wTargetString.c_str(), ntohs(ServiceName), wTestDomain.c_str());
				}
				else {
					if (TargetString == FQDN.get())
					{
						wprintf(L"DNSPing %ls:%u with %ls:\n", wTargetString.c_str(), ntohs(ServiceName), wTestDomain.c_str());

					//Output to file.
						if (OutputFile != nullptr)
							fwprintf_s(OutputFile, L"DNSPing %ls:%u with %ls:\n", wTargetString.c_str(), ntohs(ServiceName), wTestDomain.c_str());
					}
					else {
						std::shared_ptr<wchar_t> wFQDN(new wchar_t[strlen(FQDN.get())]());
						MultiByteToWideChar(CP_ACP, NULL, FQDN.get(), MBSTOWCS_NULLTERMINATE, wFQDN.get(), (int)strlen(FQDN.get()));
						wprintf(L"DNSPing %ls:%u [%ls] with %ls:\n", wFQDN.get(), ntohs(ServiceName), wTargetString.c_str(), wTestDomain.c_str());

					//Output to file.
						if (OutputFile != nullptr)
							fwprintf_s(OutputFile, L"DNSPing %ls:%u [%ls] with %ls:\n", wFQDN.get(), ntohs(ServiceName), wTargetString.c_str(), wTestDomain.c_str());
					}
				}
			}
			else {
				wprintf(L"DNSPing %ls:%u [%ls] with %ls:\n", wTargetDomainString.c_str(), ntohs(ServiceName), wTargetString.c_str(), wTestDomain.c_str());

			//Output to file.
				if (OutputFile != nullptr)
					fwprintf_s(OutputFile, L"DNSPing %ls:%u [%ls] with %ls:\n", wTargetDomainString.c_str(), ntohs(ServiceName), wTargetString.c_str(), wTestDomain.c_str());
			}
		}
		else {
			if (!TargetDomainString.empty())
			{
				wprintf(L"DNSPing %ls:%u [%ls] with %ls:\n", wTargetDomainString.c_str(), ntohs(ServiceName), wTargetString.c_str(), wTestDomain.c_str());

			//Output to file.
				if (OutputFile != nullptr)
					fwprintf_s(OutputFile, L"DNSPing %ls:%u [%ls] with %ls:\n", wTargetDomainString.c_str(), ntohs(ServiceName), wTargetString.c_str(), wTestDomain.c_str());
			}
			else {
				wprintf(L"DNSPing %ls:%u with %ls:\n", wTargetString.c_str(), ntohs(ServiceName), wTestDomain.c_str());

			//Output to file.
				if (OutputFile != nullptr)
					fwprintf_s(OutputFile, L"DNSPing %ls:%u with %ls:\n", wTargetString.c_str(), ntohs(ServiceName), wTestDomain.c_str());
			}
		}

	//Send.
		if (SendNum == 0)
		{
			while (true)
			{
				if (RealSendNum <= U16_MAXNUM)
				{
					RealSendNum++;
					if (SendProcess(SockAddr) == EXIT_FAILURE)
					{
						WSACleanup();
						return EXIT_FAILURE;
					}
				}
				else {
					wprintf(L"\nStatistics is full.\n");
				//Output to file.
					if (OutputFile != nullptr)
						fwprintf_s(OutputFile, L"\nStatistics is full.\n");

					PrintProcess(true, true);
				//Close file handle.
					if (OutputFile != nullptr)
						fclose(OutputFile);

					WSACleanup();
					return EXIT_SUCCESS;
				}
			}
		}
		else {
			for (size_t Index = 0;Index < SendNum;Index++)
			{
				RealSendNum++;
				if (SendProcess(SockAddr) == EXIT_FAILURE)
				{
				//Close file handle.
					if (OutputFile != nullptr)
						fclose(OutputFile);

					WSACleanup();
					return EXIT_FAILURE;
				}
			}
		}

	//Print to screen before finished.
		PrintProcess(true, true);

	//Close file handle.
		if (OutputFile != nullptr)
			fclose(OutputFile);
	}
	else {
		PrintDescription();
	}

	WSACleanup();
	return EXIT_SUCCESS;
}
