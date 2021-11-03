//Déclaration des librairies nécessaires
#include "WiFiEsp.h"      //Librairie pour l'utilisation du WiFi
#include <PubSubClient.h> //Librairie pour l'utilisation de MQTT
#include "DHT.h"          // Librairie des capteurs DHT

// Emulate Serial1 on pins 6/7 if not present for ESP01
#ifndef HAVE_HWSERIAL1
#include "SoftwareSerial.h"
SoftwareSerial Serial1(6, 7); // RX, TX
#endif

// Déclaration des variables et des constantes
int PhotoR = A1;  // entrée analogique de la photorésistance
int D2 = 2;       //Moteur montée
int D3 = 3;       //Moteur descente
int pinLed1 = 4;  //LED verte témoin fonctionnement
int pinLed2 = 5;  //LED rouge témoin moteur et watchdog
int FinCHaut = 8; //Entrée du fin de course HAUT. Une broche sur la masse l'autre sur l'arduino
int FinCBas = 9;  //Entrée du fin de course BAS. Une broche sur la masse l'autre sur l'arduino
int Compteur_jour = 0;  //Comptage nu nombre de valeurs lues de jour
int Compteur_nuit = 0;  //Comptage du nombre de valeurs lues de nuit
boolean Porte_ouverte=1;//A la fin du calibrage, la porte est ouverte !!!
int PhotoRLue;     //Variable pour stocker la valeur lue de la LDR
int Seuil = 350;   //Seuil de détection jour-nuit. Valeur supérieure = jour
long Tps_mvt=0;    //Pour limiter le temps de mouvement de la porte (sécurité)
int Tps_Ouv=4500;  //Le temps maximum durant lequel la porte peut bouger (sera étalonné automatiquement à l'allumage)
int Tps_Ferm=4500; //Le temps maximum durant lequel la porte peut bouger (sera étalonné automatiquement à l'allumage)
char ssid[] = "XXXXXX";      // your network SSID (name)
char pass[] = "YYYYYYYY";    // your network password
int status = WL_IDLE_STATUS;  // the Wifi radio's status
//char message_buff[100];       //Buffer qui permet de décoder les messages MQTT reçus


//Propriétés du serveur MQTT
#define mqtt_server "XXX.XXX.XXX.XXX"
#define mqtt_user "XXXXX"  //Nom d'utilisateur MQTT s'il a été configuré sur Mosquitto
#define mqtt_password "YYYYYYY" //Mot de passe MQTT
#define humidity_topic "gladys/master/device/mqtt:poulailler:capteur-temperature/feature/mqtt:poulailler:capteur-temperature:humidite/state"  //Topic température
#define temperature_topic "gladys/master/device/mqtt:poulailler:capteur-temperature/feature/mqtt:poulailler:capteur-temperature:temperature/state"        //Topic humidité
#define DoorState_topic "gladys/master/device/mqtt:poulailler:capteur-temperature/feature/mqtt:poulailler:capteur-temperature:Ouverture_Porte/state"        //Topic état de la porte
#define LDR_topic "gladys/master/device/mqtt:poulailler:capteur-temperature/feature/mqtt:poulailler:capteur-temperature:Luminosite/state" //Topic luminosité

//Propriétés du capteur DHT
#define DHTPIN 10 //Broche du DHT
#define DHTTYPE DHT11 // Type du DHT (11 ou 22)
//Création des objets
DHT dht(DHTPIN, DHTTYPE);
WiFiEspClient espClient;
PubSubClient client(espClient);

void setup()
{
  // démarrage la liaison série entre entrée analogique et ordi
  Serial.begin(9600);
  //Déclaration des contacts fin de course en entrée avec utilisation de la fonction PULLUP interne
  pinMode(FinCHaut,INPUT_PULLUP); //Les Pull Up sont des résistances internes à l'arduino.
  //Donc de base lorsque le bouton n'est pas appuyé on lit un état haut (5V = niveau logique 1)
  pinMode(FinCBas,INPUT_PULLUP);
  digitalWrite(pinLed1,HIGH);//On allume la LED témoin de fonctionnement

  // initialize serial for ESP module
  Serial1.begin(9600);
  // initialize ESP module
  WiFi.init(&Serial1);
  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield non détecté");
    digitalWrite(pinLed1,LOW);  //Problème avec le shield du WiFI. On éteind la LED.
    // don't continue
    while (true);
  }
  // attempt to connect to WiFi network
  while ( status != WL_CONNECTED) {
 //   Serial.print("Tentative de connexion au réseau WPA SSID : ");
 //   Serial.println(ssid);
    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);
  }
//  Serial.println("Connection au réseau réussie");
 
  // Connexion au serveur MQTT
  client.setServer(mqtt_server, 1883);    //Configuration de la connexion au serveur MQTT
/* client.setCallback(callback);  //La fonction de callback qui est executée à chaque réception de message - Inutilisée ici   
   while (!client.connected()) {
 //   Serial.print("Connexion à Gladys en MQTT...");
    if (client.connect("Poulailler_TH", mqtt_user, mqtt_password )) {
 //     Serial.println("Connecté !!");
    } else {
      Serial.print("Erreur ! Code : ");
      Serial.print(client.state());
      delay(100);
      }
  }*/
  dht.begin();

//Fonction d'initialisation de la porte (calibrage du temps d'ouverture/fermeture). D'abord, on ouvre la porte
 //Si interrrupteur haut pas enfoncé, ouvrir (monter) porte
 if (digitalRead(FinCHaut) == 1){
// Serial.println("Séquence d'initialisation. Ouverture de la porte...");
 Tps_mvt=millis() + Tps_Ouv;
 while((digitalRead(FinCHaut) == 1) && (Tps_mvt>=millis()))
 {
 //Ouverture
      digitalWrite(D2,HIGH);
      digitalWrite(D3,LOW);
      digitalWrite(pinLed2,HIGH); //Ca s'ouvre, led rouge allumée
    }
  digitalWrite(D2,LOW);//On arrête le moteur
  digitalWrite(pinLed2,LOW); //On éteint la LED
  }
//  Serial.print("La porte est ouverte !");
  //Maintenant, on referme et on compte combien de temps il faut
//  Serial.println("Etalonnage du temps de fermeture...");
  Tps_mvt=millis() + Tps_Ferm;
  Tps_Ferm=millis(); 
//  Serial.println("Fermeture de la porte...");   
  while((digitalRead(FinCBas) == 1) && (Tps_mvt>=millis()))
 {
 //Fermeture
      digitalWrite(D3,HIGH);
      digitalWrite(D2,LOW);
      digitalWrite(pinLed2,HIGH); //Ca se ferme, LED rouge allumée
    }
  digitalWrite(D3,LOW);//On arrête le moteur
  digitalWrite(pinLed2,LOW); //On éteint la LED
  Tps_Ferm=millis() - Tps_Ferm + 1000;//On stocke le temps nécessaire (+ marge d'une seconde)
/*  Serial.print("Le temps nécessaire à la fermeture est de : ");
  Serial.print(Tps_Ferm);
  Serial.println("ms");
  //On réouvre pour mesurer le temps d'ouverture
  Serial.println("Etalonnage du temps d'ouverture...");*/
  Tps_mvt=millis() + Tps_Ouv;
  Tps_Ouv=millis();
//  Serial.println("La porte s'ouvre...");
 while((digitalRead(FinCHaut) == 1) && (Tps_mvt>=millis()))
 {
 //Ouverture
      digitalWrite(D2,HIGH);
      digitalWrite(D3,LOW);
      digitalWrite(pinLed2,HIGH); //Ca s'ouvre, LED rouge allumée
    }
  digitalWrite(D2,LOW);//On arrête le moteur
  digitalWrite(pinLed2,LOW); //On éteint la LED
//  Serial.println("La porte est ouverte");
  Tps_Ouv=millis() - Tps_Ouv + 1000;
/*  Serial.print("Le temps d'ouverture nécessaire est de : ");
  Serial.print(Tps_Ouv);
  Serial.println("ms");*/
}

void Fermeture() //Fonction utilisée pour fermer la porte
{
//  Serial.println("Il fait nuit, la porte se ferme");
  digitalWrite(pinLed1,LOW); //Led verte éteinte
  Tps_mvt=millis() + Tps_Ferm;
    while((digitalRead(FinCBas) == 1) && (Tps_mvt>=millis())){  //Tant que la porte n'est pas fermée et qu'il tourne depuis, le moteur tourne   
          //Fermeture
        digitalWrite(D2,LOW);
        digitalWrite(D3,HIGH);
        digitalWrite(pinLed2,HIGH); //Ca se ferme, led rouge allumée
        }
      digitalWrite(D3,LOW);//On arrete le moteur car le contact fin de course est activé
      digitalWrite(pinLed1,HIGH);//C'est fermé, Led verte allumée
      digitalWrite(pinLed2,LOW); //led rouge éteinte
      Porte_ouverte = 0;//On change le statut : la porte est fermée
      Compteur_nuit = 0;//On réinitialise les compteurs
      Compteur_jour = 0;
}

void Ouverture() //Fonction utilisée pour ouvrir la porte
{
//  Serial.println("Il fait jour, la porte s'ouvre");
  digitalWrite(pinLed1,LOW); //Led verte éteinte
  Tps_mvt=millis() + Tps_Ouv;
    while((digitalRead(FinCHaut) == 1) && (Tps_mvt>=millis())){  //Tant que la porte n'est pas ouverte, le moteur tourne   
          //Ouverture
        digitalWrite(D2,HIGH);
        digitalWrite(D3,LOW);
        digitalWrite(pinLed2,HIGH); //Ca s'ouvre, led rouge allumée
      }
    digitalWrite(D2,LOW);//On arrête le moteur
    digitalWrite(pinLed1,HIGH); //C'est ouvert, Led verte allumée
    digitalWrite(pinLed2,LOW); //led rouge éteinte
    Porte_ouverte = 1;//On change le statut : la porte est ouverte
    Compteur_jour = 0;//On réinitilise les compteurs
    Compteur_nuit = 0;
}

void reconnexion_mqtt() //Reconnexion au serveur MQTT. Attention que la boucle a été supprimée (si le réseau est inaccessible, le programme doit continuer à tout prix !
{
  if ( status != WL_CONNECTED) {
//    while ( status != WL_CONNECTED) {
      status = WiFi.begin(ssid, pass);
//    }
   }
  
    if (!client.connected()) 
    {
 //     Serial.println("Tentative de reconnexion à Gladys en MQTT...");
      String clientId = "Poulailler_TH-";
      clientId += String(random(0xffff), HEX);
      if (client.connect(clientId.c_str(), mqtt_user, mqtt_password )) 
      {
        Serial.println("Connecté");
        //client.subscribe("inTopic");
      } else 
      {
        Serial.print("Connexion à Gladys impossible, code d'erreur : ");
        Serial.println(client.state());
  //      Serial.println("Nouvelle tentative dans 5 secondes");
        delay(5000);
      }
    }
  
//  client.loop(); 
}


void loop(){
   digitalWrite(pinLed2,HIGH); //La LED clignote --> Mesures faites (watchdog)
   delay (500);
   digitalWrite(pinLed2,LOW);
   
   PhotoRLue = analogRead(PhotoR);//Lecture de la valeur de la LDR
   Serial.print("Valeur de la LDR : ");
   Serial.println(PhotoRLue);//Valeur de la LDR affichée sur le port série
    if((PhotoRLue) >= (Seuil)){//Si la luminosité est supérieure ou égale à la valeur seuil, on incrémente le compteur de jour
        Compteur_jour += 1;
    }
    if((((Porte_ouverte)==(0))) && (((Compteur_jour)==(20)))){//Si la porte est fermée et qu'on a compté 20x des valeurs>seuil --> on ouvre la porte
        Ouverture();
    }
    if((PhotoRLue) < (Seuil)){//Si la luminosité est inférieure à la valeur seuil, on incrémente le compteur de nuit
        Compteur_nuit += 1;
    }
    if((((Porte_ouverte)==(1))) && (((Compteur_nuit)==(20)))){//Si la porte est ouverte et qu'on a compté 20x des valeurs<seuil --> on ferme la porte
        Fermeture();
    }
/*Serial.println("Compteurs :");
Serial.print("Jour : ");
Serial.println(Compteur_jour);
Serial.print("Nuit : ");
Serial.println(Compteur_nuit);
*/   //Lecture de l'humidité ambiante
    float h = dht.readHumidity();
    // Lecture de la température en Celsius
    float t = dht.readTemperature();
/*    //Inutile d'aller plus loin si le capteur ne renvoit rien
    if ( isnan(t) || isnan(h)) 
    {
      Serial.println("Echec de lecture ! Vérifiez votre capteur DHT");
      digitalWrite(pinLed1,LOW); //Problème avec le DHT, on éteind la LED
      return;
    }*/
//    Serial.print("Température : ");
    t=t-3; //Ajustement de la température
/*    Serial.print(t);
    Serial.print("°C | Humidité : ");
    Serial.print(h);
    Serial.println("%");
 */   if (!client.connected()) {
    reconnexion_mqtt();
    } // Vérifie la connexion avec le serveur MQTT
//    Serial.println("Publication sur Gladys");
    client.publish(temperature_topic, String(t).c_str(), true);   //Publie la température sur le topic temperature_topic
    client.publish(humidity_topic, String(h).c_str(), true);      //Et l'humidité
    client.publish(DoorState_topic, String(digitalRead(FinCBas)).c_str(), true); //Et l'état d'ouverture de la porte
    client.publish(LDR_topic, String(PhotoRLue).c_str(), true); //Et la luminosité
    
    client.disconnect();  //On se déconnecte de Gladys
    delay (30000);//Délai d'attente entre chaque mesure de luminosité, température et hygrométrie
}
