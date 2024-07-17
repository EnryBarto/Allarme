#ifndef Sensore_h

    #define Sensore_h
    #include "Arduino.h" 

    /* CLASSE: Sensore
     *
     * Descrizione:
     *   La classe gestisce un sensore di movimento. Onde evitare false
     *   rilevazioni legge il sensore due volte a distanza di
     *   OFFSET_CONTROLLO_SENSORE millisecondi, tempo in cui il flag
     *   _in_rilevazione rimane settato a true. Se la rilevazione ha avuto
     *   esito positivo, il flag _alm viene settato a true.
     *   pausaRilevazione() serve per sapere se dall'ultima rilevazione sono
     *   già passati INTERVALLO_CONTROLLO_SENSORE millisecondi.
     */

    class Sensore {

        public:
            static const uint32_t OFFSET_CONTROLLO_SENSORE = 100;
            static const uint32_t INTERVALLO_CONTROLLO_SENSORE = 5000;
            char nome[20];
            Sensore(int gpio, const char* nome);
            void setGpio(int pin);
            int getGpio();
            void setNome(const char *n);
            void aggiornaStato();
            bool getAllarme();
            void reset();
            bool pausaRilevazione();

        private:
            int _gpio; // Pin GPIO del sensore

            bool _alm; // true quando è confermato che il sensore ha rilevato
                       // un movimento, viene impostato dalla aggiornaStato

            bool _in_rilevazione; // Rimane a true per tutto il tempo OFFSET

            uint32_t _inizio_controllo;   // Inizio del controllo del sensore
                                          // per poter aspettare il tempo OFFSET

            uint32_t _ultima_rilevazione; // Ultimo istante in cui il sensore è
                                          // stato letto

    };

#endif