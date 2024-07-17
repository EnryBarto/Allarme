# CAP v1.0
_Cetti Allarms Protocol_

In questo documento viene descritto il protocollo di comuncazione utilizzato dall'allarme

## Stati
* **SPENTO**: L'allarme è spento e non esegue nulla
* **ACCESO**: L'allarme è in funzione e controlla se i sensori rilevano qualche movimeto
* **ALLARME**: Sono stati rilevati dei movimenti e l'allarme non è stato disattivato nel tempo stabilito. Viene accesa la sirena.
* **ACCENSIONE**: L'allarme esegue il countdown per passare allo stato _ACCESO_, in modo da garantire del tempo per uscire di casa dopo averne richiesto l'accensione.
* **MOVIMENTO**: Un sensore ha rilevato un movimento: Prima di passare allo stato _ALLARME_ esegue un countdown per dare tempo all'utente di spegnerlo tramite l'interfaccia.

## Comunicazione

Nelle descrizioni verranno utilizzate le seguenti abbreviazioni:
* **A**: Allarme, Server UDP
* **I**: Interfaccia, Client UDP
* **REQ**: Messaggio di richiesta
* **RESP**: Messaggio di risposta

### Richiesta dello stato all'allarme
* REQ (I → A): "STATO"
* RESP (A → I): 
    * "OFF": Stato SPENTO
    * "ON": Stato ACCESO
    * "ALRM": Stato ALLARME
    * "ACC_x": Stato ACCENSIONE per altri x secondi
    * "MOV_x": Stato MOVIMENTO per altri x secondi

### Richiesta accensione allarme con countdown
* REQ (I → A): "_{Password}_"
* RESP (A → I):
    * "OK_ACCENSIONE": L'allarme è passato in stato ACCENSIONE
    * "NO_ACCENSIONE": L'allarme è in uno stato che non permette di passare allo stato ACCENSIONE

### Richiesta accensione immediata allarme
* REQ (I → A): "_{Password}_"
* RESP (A → I):
    * "OK_ACCESO": L'allarme è passato in stato ACCESO
    * "NO_ACCESO": L'allarme è in uno stato che non permette di passare allo stato ACCESO

### Richiesta spegnimento allarme
* REQ (I → A): "_{Password}_"
* RESP (A → I):
    * "OK_SPENTO": L'allarme è passato in stato SPENTO
    * "NO_SPENTO": L'allarme è già in stato SPENTO

