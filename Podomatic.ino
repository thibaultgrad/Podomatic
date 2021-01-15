/*
 Name:		Pediluve.ino
 Created:	11/06/2020 19:50:29
 Author:	TG
*/
//#include <SH1106Lib.h>
//#include <Adafruit_SH1106.h>
//#include <U8glib.h>
#include <NanoOLED.h>
#include <EEPROM.h>
#include <Wire.h>
#include <avr/wdt.h>
//#include "glcdfont.h"
/* Constantes pour les broches */
#define TRIGGER_PIN_PRESENCE 7 // Broche TRIGGER
#define ECHO_PIN_PRESENCE 6   // Broche ECHO
#define TRIGGER_PIN_NIVEAU 8
#define ECHO_PIN_NIVEAU 3

/*Echantillonnage*/
#define MS_MOYENNE 200
// periode d'échantillonnage en ms
#define MS_PERIOD_ECH  2

#define TAILLE_TABLEAU_ECHANTILLONS   (MS_MOYENNE / MS_PERIOD_ECH)
int i = 0;
int indexechantilon = 0;
float echantillons[TAILLE_TABLEAU_ECHANTILLONS] = { 0 };
float d_moyenne = 0l;
unsigned long loop_timer = 0UL;
unsigned long start_pulse_time = 0UL;
unsigned int _consecutiveerrors = 0u;
bool Probleme_capteur = false;

boolean etat_spray;

#define pin_moteur_relais1 10
#define pin_moteur_relais2 9

#define pinCourant A2
#define pinaAdjustValueY A0
#define pinaAdjustValueX A1

enum Affichages
{
	Tps_spray,
	Distances,
	D_mesure,
	Niveau_min_cuve,
	Nb_total_passages,
	Tps_total_spray_min,
	Tps_total_spray_h,
	Info_spray,
	Niveau_cuve_bas,
	Fin_reglage,
};
enum Etats
{
	Attente,
	Spraying,
	Erreur,
	Niveau_produit_bas,
};
int buttonpin = 4;
bool buttonstate = false;
bool prevbuttonstate = false;
int prev_vue = 1;
bool issetting = false;
bool initialised = false;
long t_debut_etat;
long duree_etat;
bool capteur_niveau = false; // utilisation ou non du capteur de niveau

Etats etat = Attente; //0=attente , 1= spray, 2=erreur
//unsigned int temps_total_spray = 0; // en minutes

//Information presente sur écran, Tps_spray,Distances,D_mesure,Nb_total_passages,Tps_total_spray_min,Tps_total_spray_h,Info_spray
Affichages Affichage_ecran = D_mesure;

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define NANOLED_PRINTF 
NanoOLED display(SH1106);

/** Le nombre magique et le numéro de version actuelle */
static const unsigned long STRUCT_MAGIC = 123456789;
static const byte STRUCT_VERSION = 2;

int _screenrefinc = 0;

struct SavedDatasStruct
{
	unsigned long magic;
	byte struct_version;
	unsigned int temps_total_spray;
	unsigned long nb_total_passage;
	unsigned int D_Min_mm;
	unsigned int D_Max_mm;
	unsigned long MS_SPRAY;
	unsigned int MS_RETARD_DEMARRAGE;
	unsigned int MS_Arret;
	unsigned int D_Min_level_cuve;
};

SavedDatasStruct SavedDatas;
/* Constantes pour le timeout */
const unsigned long MEASURE_TIMEOUT = 25000UL; // 25ms = ~8m à 340m/s

/* Vitesse du son dans l'air en mm/us */
const float SOUND_SPEED = 340.0 / 1000;

long DebutPasMesure = 0;

// the setup function runs once when you press reset or power the board
void setup() {

	//wdt_disable();
	//wdt_enable(WDTO_2S);
	pinMode(pin_moteur_relais1, OUTPUT);
pinMode(pin_moteur_relais2, OUTPUT);
digitalWrite(pin_moteur_relais1, 1);
digitalWrite(pin_moteur_relais2, 1);

//pinMode(buttonpin, INPUT_PULLUP);
//pinMode(pinaAdjustValueX, INPUT);
//pinMode(pinaAdjustValueY, INPUT);
///* Initialise les broches */
//pinMode(LED_BUILTIN, OUTPUT);
pinMode(TRIGGER_PIN_PRESENCE, OUTPUT);
//pinMode(TRIGGER_PIN_NIVEAU, OUTPUT);
digitalWrite(TRIGGER_PIN_PRESENCE, LOW); // La broche TRIGGER doit être à LOW au repos
//digitalWrite(TRIGGER_PIN_NIVEAU, LOW); // La broche TRIGGER doit être à LOW au repos
pinMode(ECHO_PIN_PRESENCE, INPUT);
//pinMode(ECHO_PIN_NIVEAU, INPUT);

//analogReference(EXTERNAL);
//Wire.begin();
//display.init();
//delay(20);
//display.print("test");
//display.clearDisplay();
t_debut_etat = millis();
etat_spray = 0;

/* Initialise le port série */
//Serial.begin(115200);
//Serial.begin(9600);
loop_timer = millis();
LoadDatas();
d_moyenne = 0;
//display.clearDisplay();

}


void loop() {

	duree_etat = abs(millis() - t_debut_etat);
	int val_etat = (int)etat;
	Echantillonnage(Mesure(TRIGGER_PIN_PRESENCE, ECHO_PIN_PRESENCE));
	//Serial.println(d_moyenne);
	switch (val_etat)
	{
	case (int)Attente: {
		//Echantillonnage(Mesure(TRIGGER_PIN_PRESENCE, ECHO_PIN_PRESENCE));
		//Serial.println(d_moyenne);
		if ((d_moyenne >= SavedDatas.D_Min_mm && d_moyenne <= SavedDatas.D_Max_mm) && duree_etat > SavedDatas.MS_Arret)
		{
			//if (capteur_niveau) {
			//	if (Mesure(TRIGGER_PIN_NIVEAU, ECHO_PIN_NIVEAU) > SavedDatas.D_Min_level_cuve) {
			//		etat = Niveau_produit_bas;
			//	}
			//	else { goto Spray; }
			////}
			////else {
			//	Serial.print(d_moyenne);
			//Spray:
			etat = Spraying;
			SprayOn();
			//Serial.println(d_moyenne);
			//Serial.println(SavedDatas.D_Min_mm);
			//Serial.println(SavedDatas.D_Max_mm);

			//Affichage_ecran = Info_spray;
		//}
		}
		break;

	}
	case (int)Spraying: {
		if (duree_etat > SavedDatas.MS_SPRAY)
		{
			etat = Attente;
			ajout_temps_spraying();
			SprayOff();
			//Affichage_ecran = D_mesure;
		}

		break;

	}
	case (int)Erreur: {
		//Echantillonnage(Mesure(TRIGGER_PIN_PRESENCE, ECHO_PIN_PRESENCE));
		break;

	}
	case (int)Niveau_produit_bas: {
		if(!issetting) Affichage_ecran = Niveau_cuve_bas;
		if (!capteur_niveau | Mesure(TRIGGER_PIN_NIVEAU, ECHO_PIN_NIVEAU) < SavedDatas.D_Min_level_cuve)
		{
			etat = Attente;
		}
		break;

	}
	default: {
		break;

	}
	}
	//buttonstate = digitalRead(buttonpin);
	//digitalWrite(13, buttonstate);

	//if (prevbuttonstate != buttonstate) {
	//	if (buttonstate == false) {
	//		int val = (int)Affichage_ecran + 1;
	//		Affichage_ecran = (Affichages)val;
	//		//display.clearDisplay();
	//		//Serial.println(Affichage_ecran);
	//		issetting = 1;
	//	}
	//}
	//prevbuttonstate = buttonstate;
	//gestion_affichage(map(analogRead(pinaAdjustValueY), 0, 1023, -1000, 1000));

	/* Délai d'attente pour éviter d'afficher trop de résultats à la seconde */
	while(abs(loop_timer - millis()) <MS_PERIOD_ECH)
	{		
	}
	loop_timer = millis();
	//wdt_reset();
}
float Mesure(int trrig_pin, int echo_pin)
{
	/* 1. Lance une mesure de distance en envoyant une impulsion HIGH de 10µs sur la broche TRIGGER */
	digitalWrite(trrig_pin, HIGH);
	delayMicroseconds(10);
	digitalWrite(trrig_pin, LOW);


	/* 2. Mesure le temps entre l'envoi de l'impulsion ultrasonique et son écho (si il existe) */
	long measure = pulseIn(echo_pin, HIGH, MEASURE_TIMEOUT);

	//Serial.println(measure / 2 * SOUND_SPEED);

	/* 3. Calcul la distance à partir du temps mesuré */
	return measure / 2.0 * SOUND_SPEED;
}
void Echantillonnage(float distance_mm)
{
	if (distance_mm > 0) {

		//echantillons[indexechantilon] = distance_mm;
		//for (i = 0; i < TAILLE_TABLEAU_ECHANTILLONS; i++)
		//{
		//	d_moyenne += echantillons[i];
		//}
		//d_moyenne = d_moyenne / TAILLE_TABLEAU_ECHANTILLONS;
		//indexechantilon++;
		//if (indexechantilon == TAILLE_TABLEAU_ECHANTILLONS)
		//{
		//	indexechantilon = 0;
		//}
		d_moyenne = distance_mm;
		_consecutiveerrors = 0;
		Probleme_capteur = false;
	}
	else
	{
		if (abs(millis() - DebutPasMesure) > 2000)d_moyenne = 0;
			DebutPasMesure = millis();
		//_consecutiveerrors += 1;
		//Probleme_capteur = true;
		//etat = Erreur;
	}
}
void LoadDatas()
{
	//Serial.println(SavedDatas.nb_total_passage);
	//Serial.println(SavedDatas.temps_total_spray);
	bool raz = true;
	//while (digitalRead(buttonpin) && loop_timer + 30000000l > micros())
	//{
	//	int val = map(analogRead(pinaAdjustValueY), 0, 1023, -1000, 1000);
	//	if (val > 500) raz = true;
	//	if (val < -500) raz = false;
	//	Affichage("Vider la memoire ?:", raz ? "Oui" : "Non");
	//	delay(5);
	//	wdt_reset();
	//}
	buttonstate = digitalRead(buttonpin);
	prevbuttonstate = buttonstate;
	if (raz) {
		for (int i = 0; i < EEPROM.length(); i++) {
			EEPROM.write(i, 0);
		}
	}
	chargeEEPROM();
}
void SprayOff()
{
	t_debut_etat = millis();
	d_moyenne = 0;
	digitalWrite(pin_moteur_relais1, HIGH);
	//delay(2);
	//digitalWrite(pin_moteur_relais2, HIGH);
	etat_spray = 0;
	//for (int i = 0; i < TAILLE_TABLEAU_ECHANTILLONS; i++)
	//{
	//echantillons[i] = { 0 };
	//}
	//indexechantilon = 0;
	//d_moyenne = 0.0;

}
void SprayOn()
{
	t_debut_etat = millis();
	d_moyenne = 0;
	etat_spray = 1;
	delay(SavedDatas.MS_RETARD_DEMARRAGE);
	digitalWrite(pin_moteur_relais1, LOW);
	//delay(2);
	//digitalWrite(pin_moteur_relais2, LOW);
}
void ajout_temps_spraying()
{
	SavedDatas.temps_total_spray += (duree_etat) / (1000000 * 60);
	SavedDatas.nb_total_passage += 1;
	sauvegardeEEPROM();
}
//void gestion_affichage(long analogvalue)
//{
//	//display.clearDisplay();
//	prev_vue = Affichage_ecran;
//	if (abs(analogvalue) < 300)
//	{
//		analogvalue = 0;
//	}
//	display.setCursor(0, 0);
//	_screenrefinc += 1;
//	if (_screenrefinc > 50)
//	{
//		display.clearDisplay();
//		_screenrefinc = 0;
//	}
//
//	int val = (int)Affichage_ecran;
//	switch (val)
//	{
//	case (int)Affichages::Tps_spray: {
//		SavedDatas.MS_SPRAY = SavedDatas.MS_SPRAY + analogvalue / 10;
//		SavedDatas.MS_SPRAY = constrain(SavedDatas.MS_SPRAY, 0, 120000);
//		int valx = map(analogRead(pinaAdjustValueX), 0, 1023, -1000, 1000);
//		if (abs(valx) < 300) valx = 0;
//		SavedDatas.MS_RETARD_DEMARRAGE = SavedDatas.MS_RETARD_DEMARRAGE + valx / 200;
//		SavedDatas.MS_RETARD_DEMARRAGE = constrain(SavedDatas.MS_RETARD_DEMARRAGE, 0,SavedDatas.MS_SPRAY);
//		Affichage("Tps de spray : "+ String(SavedDatas.MS_SPRAY / 1000) + " s","Retard au demarrage : " + String(SavedDatas.MS_RETARD_DEMARRAGE) + " ms" );
//		break;
//	}
//	case (int)Affichages::Distances: {
//		SavedDatas.D_Min_mm = SavedDatas.D_Min_mm + analogvalue / 200;
//		SavedDatas.D_Min_mm = constrain(SavedDatas.D_Min_mm, 0, 3500);
//		int valx = map(analogRead(pinaAdjustValueX), 0, 1023, -1000, 1000);
//		if (abs(valx) < 300) valx = 0;
//		SavedDatas.D_Min_mm = SavedDatas.D_Min_mm + valx / 200;
//		SavedDatas.D_Max_mm = constrain(SavedDatas.D_Max_mm, 0, 3500);
//		Affichage("D min : " + String(SavedDatas.D_Min_mm) + " mm", "D max : " + String(SavedDatas.D_Max_mm) + " mm");
//		break;
//	}
//	case (int)D_mesure: {
//		Affichage("Distance mesure : " + String(etat), String(d_moyenne) + " mm");//
//		break;
//	}
//	case (int)Affichages::Nb_total_passages: {
//		Affichage("Nombre total de passages : ", (String)SavedDatas.nb_total_passage );
//		break;
//	}
//	case (int)Affichages::Tps_total_spray_min:{
//		Affichage("Tps total aspersion :", (String)SavedDatas.temps_total_spray + " min");
//		break;
//	}
//	case (int)Affichages::Tps_total_spray_h: {
//		Affichage("Tps total aspersion :", String((float)(SavedDatas.temps_total_spray / 60)) + " h");
//		break;
//	}
//	case (int)Affichages::Info_spray: {
//		Affichage("Aspertion en cours, temps restant : " + String(SavedDatas.MS_SPRAY/1000- duree_etat / 1000 ) + " s","Total aspersion : " + (String)SavedDatas.temps_total_spray + " min");
//		break;
//	}
//	case (int)Affichages::Niveau_min_cuve: {
//		Affichage("Valeur de niveau mesuree : " + String(Mesure(TRIGGER_PIN_NIVEAU,ECHO_PIN_NIVEAU)) + " mm" ,"Valeur min : " + String(SavedDatas.D_Min_level_cuve) + " mm"  );
//		break;
//	}
//	case (int)Affichages::Niveau_cuve_bas: {
//		if(abs(analogvalue>200)) capteur_niveau = analogvalue > 0;
//		Affichage("La cuve de produit est quasi vide, remplir la cuve", "Desactiver le capteur de remplissage cuve ? " + String(capteur_niveau ? "Non" : "Oui"));
//		break;
//	}
//	case (int)Affichages::Fin_reglage: {
//		Affichage("Fin reglage donneees sauvegardees","Clique pour regler");
//		//Affichage_ecran = Tps_spray;
//		issetting = 0;
//		sauvegardeEEPROM();
//		break;
//	}
//		default: {
//		Affichage_ecran = Tps_spray;
//		Affichage("Default, etat :", "");
//		issetting = 0;
//		break;
//		}
//	}
//}
//void Affichage(String ligne1, String ligne2)
//{
//	display.setCursor(0, 0);
//	display.print(ligne1);
//	display.setCursor(3, 0);
//	display.print(ligne2);
//}
void sauvegardeEEPROM() {
	SavedDatas.magic = STRUCT_MAGIC;
	SavedDatas.struct_version = STRUCT_VERSION;
	EEPROM.put(0, SavedDatas);
}

void chargeEEPROM() {

	EEPROM.get(0, SavedDatas);

	// Détection d'une mémoire non initialisée
	byte erreur = SavedDatas.magic != STRUCT_MAGIC;

	// Valeurs par défaut struct_version == 0
	if (erreur) {

		// Valeurs par défaut pour les variables de la version 0
		SavedDatas.MS_SPRAY = 3000UL;
		SavedDatas.D_Max_mm = 1500;
		SavedDatas.D_Min_mm = 150;
		SavedDatas.temps_total_spray = 0;
		SavedDatas.MS_Arret = 10UL;
		SavedDatas.MS_RETARD_DEMARRAGE = 0;
		SavedDatas.nb_total_passage = 0UL;
		SavedDatas.D_Min_level_cuve = 350;
	}
	sauvegardeEEPROM();
}
