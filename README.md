# SmartCharger utils

That project doesn't work alone and brings together all the features related to the SmartCharger:
* Web HMI to configure and use final charger,
* WebSockets services to communicate with Web HMI and TIC Module,
* Smart charge algorithm,
* Primitives functionalities to interact with hardware.

Indeed, that repo is agnostic and must be built with ad hoc hardware:
* [Viridian_SmartCharger](https://github.com/bastoon577-lang/Viridian_SmartCharger) for building with [Viridian system](https://www.amazon.fr/Viridian-EV-EPC20L10-Contr%C3%B4leur-Blanc/dp/B097BTRGYL),
* [ET3K_SmartCharger](https://github.com/bastoon577-lang/ET3K_SmartCharger) for building with [ETEK system](https://fr.aliexpress.com/item/1005003433694455.html?dp=Cj0KCQjwj47OBhCmARIsAF5wUEExlk60GLalFcOgMwSaBVl6OryrBLvnEhBZIN-G9wakJbBSW7m_WWwaArpIEALw_wcB@251641&isdl=y&aff_fsk=_on9NVyV&src=Delupe3&aff_platform=aff_feeds&aff_short_key=_on9NVyV&pdp_npi=4%40dis%21EUR%2120.96%2120.79%21%21%21%21%21%40%2112000034155484243%21afff%21%21%21&gatewayAdapt=glo2fra&cn=251641&cv=461959&af=).

 ###### Auteur : *Sébastien DALIGAULT*. 
