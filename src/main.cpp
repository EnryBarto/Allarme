/*
 * Autore:          EnryBarto
 * Progetto:        Allarme
 * Versione:        2.0
 * Versione CAP:    1.0
 * Ultima modifica: 17/07/2024
 */


/* ------------------------- IMPORTAZIONE  LIBRERIE ------------------------- */
#include <list>
#include <EEPROM.h>
#include <Arduino.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>

#include "config.h"  // File contenente le configurazioni private
#include "Sensore.h" // Classe creata per l'uso dei sensori di movimento

using namespace std;


/* --------------------------- NUOVI TIPI DI DATO --------------------------- */

/* Stati in cui si può trovare l'allarme */
typedef enum {
    SPENTO,
    ACCENSIONE,
    ACCESO,
    MOVIMENTO,
    ALLARME
} Stati;

/* Stato e attributi dell'allarme */
typedef struct {
    Stati stato;         // Stato in cui si trova l'allarme
    bool cambio_stato;   // true se è avvenuto un cambio di stato 
    bool in_connessione; // true mentre viene stabilita la connessione wi-fi

    // Tempi in cui è iniziata una certa operazione per non bloccare il codice
    uint32_t inizio_accensione;
    uint32_t inizio_movimento;
    uint32_t inizio_connessione;
} MacchinaStati;

/* Dati relativi a un host UDP */
typedef struct {
    IPAddress ip;
    uint16_t port;
} UdpHost;


/* -------------------------------- SETTAGGI -------------------------------- */

#define GPIO_SIRENA 13
#define PORTA_SERVER_UDP 4210
#define DATAGRAM_SIZE 255       // Grandezza massima del datagramma UDP

#define MAX_RICHIESTE_UDP 100   // Numero massimo di richieste UDP consecutive
                                // che il server elabora per non tenere
                                // bloccato troppo tempo l'allarme

#define TIMEOUT_WIFI 20000      // Tempo di attesa massimo per stabilire la 
                                // connessione al WiFi con successo (in ms)

#define TEMPO_ACCENSIONE 30000  // Tempo di attesa per passare dallo stato
                                // ACCENSIONE allo stato ACCESSO

#define TEMPO_SPEGNIMENTO 15000 // Tempo di attesa per passare dallo stato
                                // MOVIMENTO allo stato ALLARME


/* --------------------------- VARIABILI  GLOBALI --------------------------- */

list<Sensore*> sensori; // Lista dei sensori connessi all'allarme

//Array di servizio per la spedizione di messaggi tramite il bot di telegram:
// Utilizzati per eseguire le funzioni sulle stringhe
char messaggio[100];
char tempo[5];

WiFiUDP serverUdp;     // Sessione Udp per la ricezione e l'invio dei messaggi
UdpHost telegramBot;   // Dati per l'invio di datagrammi al bot di telegram 
MacchinaStati allarme; // Variabile contenente tutte gli attributi dell'allarme


/* --------------------------- PROTOTIPI FUNZIONI --------------------------- */

// Gestione allarme
void cambia_stato(Stati nuovo_stato);
bool accendi_allarme_countdown();
bool accendi_allarme_immediato();
bool spegni_allarme();
Stati leggi_EEPROM();
void aggiorna_EEPROM(Stati stato);
int tempo_rimasto_accensione();
int tempo_rimasto_movimento();

// Comunicazione di rete
void server_udp();
void messaggio_telegram(const char* message);


/* -------------------------- DEFINIZIONE FUNZIONI -------------------------- */

void setup() {

    // Lettura dello stato da cui far ripartire l'allarme dalla EEPROM
    EEPROM.begin(512);
    allarme.cambio_stato = true;
    allarme.stato = leggi_EEPROM();

    pinMode (GPIO_SIRENA, OUTPUT);
    digitalWrite(GPIO_SIRENA, allarme.stato == ALLARME ? HIGH : LOW);

    // Creazione e aggiunta dei sensori
    sensori.push_back(new Sensore(4, "Sala"));
    sensori.push_back(new Sensore(5, "Scale"));
    sensori.push_back(new Sensore(12, "Cucina"));
    sensori.push_back(new Sensore(14, "Corridoio"));

    // Dati dell'interfaccia al bot di telegram
    telegramBot.ip = IPAddress(TG_BOT_IP0, TG_BOT_IP1, TG_BOT_IP2, TG_BOT_IP3);
    telegramBot.port = 4120;

    // Inizio della connessione al WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    allarme.inizio_connessione = millis();
    allarme.in_connessione = true;
}

void loop() {

    /* CONTROLLO DELLA CONNESSIONE E GESTIONE DEI DATAGRAMMI UDP */

    if (allarme.in_connessione) {

        if (WiFi.status() == WL_CONNECTED) {
            // La connessione è stata stabilita
            serverUdp.begin(PORTA_SERVER_UDP);
            allarme.in_connessione = false;
        } else if (millis() - allarme.inizio_connessione >= TIMEOUT_WIFI) {
            // La connessione non è avvenuta entro il tempo massimo
            allarme.in_connessione = false;
        }
    } else {

        if (WiFi.status() == WL_CONNECTED) {
            // La connessione WiFi è attiva
            server_udp();
        } else {
            // La connessione WiFi è disattivata, provo a ristabilirla
            allarme.in_connessione = true;
            serverUdp.stop();
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            allarme.inizio_connessione = millis();
        }
    }

    /* CONTROLLO DEL MOVIMENTO */

    switch (allarme.stato) {

        case ACCENSIONE:
            // Aspetta il tempo preimpostato per accendere l'allarme
            if (millis() - allarme.inizio_accensione >= TEMPO_ACCENSIONE) {
                cambia_stato(ACCESO);
            }
            break;
        
        case ACCESO:
            // Aggiorna lo stato di ogni sensore, se uno di questi rileva un
            // movimento passa allo stato MOVIMENTO
            for (Sensore* sens : sensori) {
                sens->aggiornaStato();

                if (sens->getAllarme()) {
                    // Creazione del messaggio da spedire tramite telegram
                    strcpy(messaggio, sens->nome);
                    strcat(messaggio,
                        " ha rilevato un movimento, attivazione allarme tra ");
                    sprintf(tempo, "%d", TEMPO_SPEGNIMENTO/1000);
                    strcat(messaggio, tempo);
                    strcat(messaggio, " secondi");
                    messaggio_telegram(messaggio);
                    
                    cambia_stato(MOVIMENTO);
                } 
            }
            break;
        
        case MOVIMENTO:
            // Aspetta il tempo preimpostato prima di far partire l'allarme
            if (millis() - allarme.inizio_movimento >= TEMPO_SPEGNIMENTO) {
                cambia_stato(ALLARME);
            }
            break;

        case ALLARME:
            // Continua a controllare ogni sensore per avvisare tramite telegram
            for (Sensore* sens : sensori) {

                // Aspetta il tempo preimpostato prima di rimandare il messaggio
                // relativo allo stesso sensore
                if (!sens->pausaRilevazione()) {
                    
                    sens->aggiornaStato();
                    if (sens->getAllarme()) {
                        strcpy(messaggio, sens->nome);
                        strcat(messaggio, " continua a rilevare movimenti");
                        messaggio_telegram(messaggio);
                    } 
                }
            }
            break;

        default:
            break;
    }

    /* GESTIONE DEL CAMBIO DEGLI STATI:
     * Ad ogni cambio di stato aggiorna lo stato sulla EEPROM,
     * accende / spegne la sirena e avvisa del cambio tramite telegram
     */

    if (allarme.cambio_stato) {
        
        switch (allarme.stato) {

            case SPENTO:
                aggiorna_EEPROM(SPENTO);
                digitalWrite(GPIO_SIRENA, LOW);
                for (Sensore* sens : sensori) {
                    sens->reset();
                }
                messaggio_telegram("Allarme Spento");
                break;

            case ALLARME:
                aggiorna_EEPROM(ALLARME);
                digitalWrite(GPIO_SIRENA, HIGH);
                messaggio_telegram("ALLARME ATTIVATO");
                break;
            
            case ACCESO:
                aggiorna_EEPROM(ACCESO);
                digitalWrite(GPIO_SIRENA, LOW);
                messaggio_telegram("Allarme Acceso");
                break;
            
            case MOVIMENTO:
                aggiorna_EEPROM(ALLARME); 
                allarme.inizio_movimento = millis();
                break;

            case ACCENSIONE:
                aggiorna_EEPROM(ACCESO);
                sprintf(tempo, "%d", TEMPO_ACCENSIONE / 1000);
                strcpy(messaggio, "Accensione allarme tra ");
                strcat(messaggio, tempo);
                strcat(messaggio, " secondi");
                messaggio_telegram(messaggio);
                break;
        }

        allarme.cambio_stato = false;
    }
    
}

/* Funzione:    server_udp
 * Parametri:
 *   #
 * Valore ritornato:
 *   #
 * Descrizione:
 *   Legge i datagrammi UDP in ingresso (al massimo un numero pari a
 *   MAX_RICHIESTE_UDP), esegue i comandi impartiti e risponde alle richieste
 */

void server_udp () {
    int richieste = 0;
    char datagramma[DATAGRAM_SIZE];
    char risposta[DATAGRAM_SIZE];

    while (serverUdp.parsePacket() && richieste < MAX_RICHIESTE_UDP) {
        richieste++;
        int len = serverUdp.read(datagramma, DATAGRAM_SIZE);

        if (len > 0) {
            datagramma[len] = '\0';

            // Accensione dell'allarme facendo il countdown
            if (strcmp(datagramma, CODICE_ACC_COUNTDOWN) == 0) {
                if (accendi_allarme_countdown()) {
                    strcpy(risposta, "OK_ACCENSIONE");
                } else {
                    strcpy(risposta, "NO_ACCENSIONE");
                }
            }

            // Accensione immediata dell'allarme
            else if (strcmp(datagramma, CODICE_ACC_IMMEDIATA) == 0) {
                if (accendi_allarme_immediato()) {
                    strcpy(risposta, "OK_ACCESO");
                } else {
                    strcpy(risposta, "NO_ACCESO");
                }
            }

            // Spegnimento dell'allarme
            else if (strcmp(datagramma, CODICE_SPEGNIMENTO) == 0) {
                if (spegni_allarme()) {
                    strcpy(risposta, "OK_SPENTO");
                } else {
                    strcpy(risposta, "NO_SPENTO");
                }
            }

            // Richiesta dello stato dell'allarme
            else if (strcmp(datagramma, "STATO") == 0) {

                switch (allarme.stato) {
                    case SPENTO:
                        strcpy(risposta, "OFF");
                        break;
                    
                    case ACCESO:
                        strcpy(risposta, "ON");
                        break;
                    
                    case ALLARME:
                        strcpy(risposta, "ALRM");
                        break;

                    case ACCENSIONE: {
                        char tempo[5];
                        strcpy(risposta, "ACC_");
                        sprintf(tempo, "%d", tempo_rimasto_accensione());
                        strcat(risposta, tempo);
                        break;
                    }

                    case MOVIMENTO: {
                        char tempo[5];
                        strcpy(risposta, "MOV_");
                        sprintf(tempo, "%d", tempo_rimasto_movimento());
                        strcat(risposta, tempo);
                        break;
                    }
                }
            }

            // Manda la risposta costruita
            serverUdp.beginPacket(serverUdp.remoteIP(), serverUdp.remotePort());
            serverUdp.write(risposta);
            serverUdp.endPacket();

        }
    }
}


/* Funzione:    cambia_stato
 * Parametri:
 *   - nuovo_stato: Stato in cui l'allarme si troverà al prossimo loop
 * Valore ritornato:
 *   #
 * Descrizione:
 *   Imposta il nuovo stato e setta a true il flag di cambio di stato
 */

void cambia_stato(Stati nuovo_stato) {
    allarme.cambio_stato = true;
    allarme.stato = nuovo_stato;
}

/* Funzione:    tempo_rimasto_accensione
 * Parametri:
 *   #
 * Valore ritornato:
 *   Secondi rimasti per passare dallo stato ACCENSIONE a quello ACCESO
 *   -1 nel caso in cui il tempo sia già passato
 * Descrizione:
 *   Esegue il calcolo per ritornare il valore previsto
 */

int tempo_rimasto_accensione() {
    int tempo = millis() - allarme.inizio_accensione;
    tempo = TEMPO_ACCENSIONE - tempo;
    tempo /= 1000;
    
    return tempo >= 0 ? tempo : -1;
}


/* Funzione:    tempo_rimasto_movimento
 * Parametri:
 *   #
 * Valore ritornato:
 *   Secondi rimasti per passare dallo stato MOVIMENTO a quello ALLARME
 *   -1 nel caso in cui il tempo sia già passato
 * Descrizione:
 *   Esegue il calcolo per ritornare il valore previsto
 */

int tempo_rimasto_movimento() {
    int tempo = millis() - allarme.inizio_movimento;
    tempo = TEMPO_SPEGNIMENTO - tempo;
    tempo /= 1000;
    
    return tempo >= 0 ? tempo : -1;
}


/* Funzione:    accendi_allarme_countdown
 * Parametri:
 *   #
 * Valore ritornato:
 *   true o false a seconda dell'avvenuta accensione dell'allarme
 * Descrizione:
 *   Fa passare l'allarme allo stato ACCENSIONE
 */

bool accendi_allarme_countdown() {

    switch (allarme.stato) {
        case SPENTO:
            cambia_stato(ACCENSIONE);
            allarme.inizio_accensione = millis();
            return true;
        
        default:
            return false;
    }
}


/* Funzione:    accendi_allarme_immediato
 * Parametri:
 *   #
 * Valore ritornato:
 *   true o false a seconda dell'avvenuta accensione dell'allarme
 * Descrizione:
 *   Fa passare l'allarme allo stato ACCESO
 */

bool accendi_allarme_immediato() {

    switch (allarme.stato) {
        case SPENTO: case ACCENSIONE:
            cambia_stato(ACCESO);
            return true;

        default:
            return false;
    }
}


/* Funzione:    spegni_allarme
 * Parametri:
 *   #
 * Valore ritornato:
 *   true o false a seconda dell'avvenuto spegnimento dell'allarme
 * Descrizione:
 *   Fa passare l'allarme allo stato SPENTO
 */

bool spegni_allarme() {

    switch (allarme.stato) {
        case SPENTO:
            return false;
        
        default:
            cambia_stato(SPENTO);
            return true;
    }
}


/* Funzione:    leggi_EEPROM
 * Parametri:
 *   #
 * Valore ritornato:
 *   Stato in cui si trovava l'allarme prima dello spegnimento
 * Descrizione:
 *   Legge lo stato dalla EEPROM
 */

Stati leggi_EEPROM () {
    Stati lettura;
    EEPROM.get(0, lettura);
    return lettura;
}


/* Funzione:    aggiorna_EEPROM
 * Parametri:
 *   - stato: Stato in cui si trova l'allarme da scrivere sulla EEPROM
 * Valore ritornato:
 *   #
 * Descrizione:
 *   Scrive lo stato passato come parametro nella EEPROM
 */

void aggiorna_EEPROM (Stati stato) {
    EEPROM.put(0, stato);
    EEPROM.commit();
}


/* Funzione:    messaggio_telegram
 * Parametri:
 *   - testo: Testo del messaggio
 * Valore ritornato:
 *   #
 * Descrizione:
 *   Spedisce un messaggio tramite il bot di telegram contenente il testo
 *   passato come parametro alla funzione
 */

void messaggio_telegram(const char *testo) {
    serverUdp.beginPacket(telegramBot.ip, telegramBot.port);
    serverUdp.write(testo);
    serverUdp.endPacket();
}