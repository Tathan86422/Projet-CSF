import processing.serial.*;

Serial myPort;        // Définition du port série
String myString = null;
int angle = 0;
float distance = 0;
float maxDistance = 200; // Distance max du radar en cm (ajustable)

void setup() {
  size(1000, 600); // Taille de la fenêtre
  smooth();
  
  // Affiche la liste des ports disponibles dans la console du bas
  printArray(Serial.list());
  
  // ATTENTION : Change le [0] par le numéro de ton port COM dans la liste !
  // Le 115200 doit être IDENTIQUE au Serial.begin() de ton Arduino
  String portName = Serial.list()[0]; 
  myPort = new Serial(this, portName, 115200);
  myPort.clear();
}

void draw() {
  // Effet de rémanence (flou de mouvement style vieux radar)
  fill(0, 0, 0, 15); 
  noStroke();
  rect(0, 0, width, height);
  
  // Dessin de la grille du radar
  drawRadarGrid();
  
  // Dessin de la ligne de balayage et de l'obstacle
  drawRadarSweep();
}

// Lecture des données en provenance de l'Arduino
void serialEvent(Serial myPort) {
  try {
    myString = myPort.readStringUntil('\n'); // On lit jusqu'au retour à la ligne
    if (myString != null) {
      myString = trim(myString); // On enlève les espaces invisibles
      String[] data = split(myString, ','); // On sépare au niveau de la virgule
      
      if (data.length == 2) {
        angle = int(data[0]);
        distance = float(data[1]);
        
        // Si le capteur renvoie -1 (pas d'obstacle), on met la distance au max
        if (distance <= 0) {
          distance = maxDistance;
        }
      }
    }
  } catch (Exception e) {
    println("Erreur de lecture ou de conversion : " + e.getMessage());
  }
}

void drawRadarGrid() {
  pushMatrix();
  translate(width/2, height - 50); // Origine en bas au milieu
  noFill();
  strokeWeight(2);
  stroke(0, 150, 0); // Vert radar
  
  // Cercles concentriques (les distances)
  for (int i = 1; i <= 4; i++) {
    float r = ( (width - 100) / 4 ) * i;
    arc(0, 0, r, r, PI, TWO_PI);
    fill(0, 200, 0);
    textSize(12);
    text(int((maxDistance / 4) * i) + " cm", (r/2) + 5, -5);
    noFill();
  }
  
  // Lignes des angles (0°, 45°, 90°, 135°, 180°)
  for (int a = 0; a <= 180; a += 45) {
    float rad = radians(a);
    float x = (width/2 - 50) * cos(PI + rad);
    float y = (width/2 - 50) * sin(PI + rad);
    line(0, 0, x, y);
  }
  popMatrix();
}

void drawRadarSweep() {
  pushMatrix();
  translate(width/2, height - 50);
  strokeWeight(4);
  
  // Convertir l'angle en radians pour Processing
  float rad = radians(angle);
  
  // 1. Dessin de la ligne de balayage (Verte)
  stroke(0, 255, 0, 200);
  float sweepX = (width/2 - 50) * cos(PI + rad);
  float sweepY = (width/2 - 50) * sin(PI + rad);
  line(0, 0, sweepX, sweepY);
  
  // 2. Dessin de l'obstacle si la distance est inférieure au maximum
  if (distance < maxDistance) {
    stroke(255, 0, 0); // Rouge pour l'obstacle
    fill(255, 0, 0);
    
    // Règle de trois pour adapter la distance en cm aux pixels de l'écran
    float displayDist = map(distance, 0, maxDistance, 0, width/2 - 50);
    float objX = displayDist * cos(PI + rad);
    float objY = displayDist * sin(PI + rad);
    
    ellipse(objX, objY, 15, 15); // Un gros point rouge là où est l'objet
  }
  popMatrix();
  
  // Affichage des textes en haut de l'écran
  fill(0, 255, 0);
  textSize(20);
  text("FLUXVISION180°", 30, 40);
  textSize(16);
  text("Angle : " + angle + "°", 30, 70);
  text("Distance : " + (distance == maxDistance ? "---" : int(distance) + " cm"), 30, 90);
}
