# Tema 2 APD - Protocol BitTorrent

## Descriere
Scopul temei este simularea protocolului **BitTorrent** de partajare peer-to-peer folosind MPI.  
Protocolul **Bittorrent** este un protocol de partajare peer-to-peer, care permite distribuirea de
date intr-un mod descentralizat. Pentru a trimite/primi fisiere, *clientii* folosesc un *client*
**BitTorrent**. Acest *client* interactioneaza cu un *tracker*. *Tracker-ul* furnizeaza o lista de
fisiere disponibile pentru download si permite *clientului* sa gaseasca utilizatori de la care pot
transfera aceste fisiere.  
Acest protocol permite unui user sa se alature unui *swarm* pentru a descarca/incarca fisiere unul
de la altul simultan. Fisierul este impartit in segmente. Cand un nod primeste acest segment, acesta
devine o sursa pentru alti *clienti*. Fiecare segment este protejat de un hash criptografic. Acest
lucru impiedica modificarea segmentului accidentala sau malitioasa.  
In mod normal, segmentele sunt descarcate intr-o ordine non-secventiala si sun rearanjate de catre
*clientul* **BitTorrent**. In aceasta tema, descarcarea s-a facut in ordinea corecta.  

## Roluri in **BitTorrent**
- seed -> Detine fisierul in totalitate si il poate partaja. Se ocupa doar cu upload.
- peer -> Detine o parte din segmente dintr-un fisier. Se ocupa atat cu download-ul cat si cu
  upload-ul fisierului.
- leecher -> Fie nu detine segmente fie nu participa in partajarea lor. Se ocupa doar cu download-ul
  fisierului.

## Implementare

### *Tracker-ul* **BitTorrent**
*Tracker-ul* mentine o lista de fisiere si *swarm-ul* asociat fiecaruia dintre ele. *Swarm-ul* unui
fisier contine lista de *clienti* care au segmente din fisier si ce segmente are fiecare. Un segment
este definit de un hash si pozitia din fisier.  
*Tracker-ul* are rol de intermediar.  
Implementarea acestui *swarm* a fost facuta cu un hash map (filename->(hash
map) *client*->(vector) hashes).  
Initial, *tracker-ul* asteapta de la fiecare *client* lista de fisiere detinute si segmentele acestui
fisier. Acesta sunt salvate in *swarm*. Dupa primirea listei de la toti *clientii*, acesta va trimite
un mesaj de tipul "ACK" thread-urilor ce se ocupa cu download pentru a putea incepe descarcarea.  
Dupa aceasta initializare, *tracker-ul* asteapta mesaje de la *clienti*. Mesajele pot fi urmatoarele:
- "REQ" -> Un mesaj de tip request. La primirea acestui mesaj, tracker-ul va trimite clientului
  lista de *clienti* ce detin segmente din fisierul dorit dar si segmentele detinute.
- "UPD" -> Un mesaj de actualizare. La primirea acestui mesaj, *tracker-ul* va primi ultimele 10
  segmente descarcate (in timpul descarcarii). Acest update se realizeaza periodic, din 10 in 10 segmente.
- "FIN" -> Un mesaj de actualizare. La primirea acestui mesaj, *tracker-ul* va sti ca un *client* a
  terminat de descarcat un fisier si va astepta ultimele segmente din fisier (daca este cazul).
- "ALL" -> Un mesaj de actualizare. La primirea acestui mesaj, *tracker-ul* va sti ca un *client* a
  terminat de descarcat <u>toate</u> fisierele pe care le doreste. Daca toti *clientii* au terminat,
  se va trimite un mesaj de finalizare pentru thread-urile de upload.
