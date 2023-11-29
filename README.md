# Projeto Sistemas Operativos - Exercício 1

Projeto realizado por:
- João Fidalgo - Nº103471
- José Lopes - Nº103938

----

# Informação sobre os novos testes adicionados

- Create_file_with_existing_name - Testa o facto de não ser possível cirar um ficheiro com o mesmo nome de um já existente.

- Delete_opened_file - Testa o facto de não ser possível dar delete a um ficheiro aberto.

- Hardlink_to_unexistent_file - Testa o facto de não ser possível criar um hard link para um ficheiro inexistente. 

- Invalid_file_name - Testa o facto de não ser possível criar um ficheiro com um nome superior a 40 caracteres. 

- Symlink_to_symlink - Testa a criação de um symbolic link a apontar para outro symbolic link. 

- Symlink_to_unexistent_file - Testa o facto de não ser possível criar um symbolic link para um ficheiro inexistente. 

- Hardlink_from_unexistent_file_multithread - Mesmo teste só que com várias threads. 

- Symlink_simple_multithread - Testa symbolic links com várias threads.

- Copy_from_external_multithread - Testa cópias de ficheiros exteriores ao TFS com várias threads. 
