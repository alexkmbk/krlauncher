# krlauncher
Запуск службы и клиента Kerio VPN, а также подключение RDP

Для запуска, необходимо указать следующие параметры коммандной строки:

-RDPmstsc<Путь к файлу настроек подключения к удаленному столу>
-VPN<Путь к установленному VPN клиенту>
-SRV<Имя удаленного компьютера, к которому выполняется подлючение>

Пример:
keriolauncher.exe -RDP"mstsc D:\server.rdp" -VPN"D:\bin\KerioVPNClient\VPN Client\kvpncgui.exe" -SRVservername
