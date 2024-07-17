#include <Sensore.h>

Sensore::Sensore(const int gpio, const char* nome) {
    setGpio(gpio);
    setNome(nome);
    pinMode(this->getGpio(), INPUT);
    this->_alm = false;
    this->_inizio_controllo = 0;
    this->_in_rilevazione = false;
    this->_ultima_rilevazione = 0;
}

void Sensore::setGpio(int pin) {
    if (pin >= 0) _gpio = pin;
    else _gpio = -1;
}

int Sensore::getGpio() {
    return _gpio;
}

void Sensore::setNome(const char *n) {
    strcpy(nome, n);
}

/* Questo metodo serve per settare il flag _alm del sensore.
 * Nel caso di rilevazione di movimento, esegue un altro controllo
 * dopo aver aspettato almeno OFFSET millisecondi onde evitre false letture.
 */
void Sensore::aggiornaStato() {

    if (this->_in_rilevazione) {

        if (millis() - this->_inizio_controllo >= Sensore::OFFSET_CONTROLLO_SENSORE) {
            if (digitalRead(this->getGpio())) {
                this->_alm = true;
            }
            this->_in_rilevazione = false;
            this->_ultima_rilevazione = millis();
        }

    } else {
        if (digitalRead(this->getGpio())) {
            this->_in_rilevazione = true;
            this->_inizio_controllo = millis();
        } else {
            this->_in_rilevazione = false;
        }
    }
}

bool Sensore::pausaRilevazione() {
    uint32_t tempo_trascorso = millis() - this->_ultima_rilevazione;
    return tempo_trascorso <= Sensore::INTERVALLO_CONTROLLO_SENSORE;
}

/* Attenzione!
 * Una volta letto il valore di _alm, questo viene resettato
 */
bool Sensore::getAllarme() {
    if (this->_in_rilevazione) {
        return false;
    }

    if (this->_alm) {
        this->_alm = false;
        return true;
    } else {
        return false;
    }

}

void Sensore::reset() {
    this->_alm = false;
    this->_inizio_controllo = 0;
    this->_in_rilevazione = false;
    this->_ultima_rilevazione = 0;
}
