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
Tema a fost realizata in **C++**.

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

### Thread-ul de **upload**
Acest thread asteapta mesaje. Mesajele pot fi urmatoarele:
- "REQ" -> Un request de la un *client* pentru un segment anume. Va trimite segmentul.
- "DON" -> <u>Toti</u> *clientii* au terminat de descarcat fisierele dorite. Se inchide.

### Thread-ul de **download**
Acest thread se ocupa cu descarcarea de fisiere.  
La inceput, se asteapta un mesaj de tip "ACK" de la tracker pentru a incepe descarcarea de fisiere.
Se verifica daca *clientul* vrea fisiere, si daca nu va trimite un mesaj de tip "ALL" tracker-ului.
Daca clientul vrea fisiere, se vor face urmatorii pasi:
- Trimite o cerere de tip "REQ" tracker-ului pentru a primi lista de *clienti* care detin fisierul
  si segmentele fisierului. 
- Calculez dimensiunea fisieruli si incep sa trimit cereri catre thread-ul de **upload**. 
- Dupa primirea hash-ului dorit, verific daca hash-ul primit se afla in lista primita
  de la **tracker**. In cazul in care nu se afla, inseamna ca hash-ul primit a fost corupt asa ca il
  cer inca o data. Dupa primirea hash-ului corect, se adauga in lista "locala" a *clientului*. 
- Din 10 in 10 segmente, voi trimite *update* tracker-ului (Mesajul "UPD").
- Pentru a nu cere unui singur *client* toate hash-urile, se alterneaza folosind lista primita de la
  *tracker* astfel: Primul hash se cere primului *client* din lista, al doilea celui de al doilea
  ... iar cand raman fara clienti in lista o iau de la capat. Este asemanator cu ***Round Robin***.
- La final trimite un mesaj "FIN" *tracker-ului* si restul segmentelor.
- Dupa terminarea descarcarii tuturor fisierelor se trimite si mesajul "ALL" *tracker-ului*.

## Rulare
In directorul checker se ruleaza comanda:
```
./checker
```
Daca, pe masina locala este instalat docker, se poate rula din direcotrul sursa comanda:
```
./local.sh
```
<u>**ATENTIE:**</u> este necesar ca pe masina locala sa fie instalat mpich.
