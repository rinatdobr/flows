import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;

void main() {
  runApp(const ClientApp());
}

class ClientApp extends StatelessWidget {
  const ClientApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Flow DB Manager',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.blueGrey),
        useMaterial3: true,
      ),
      home: const FlowListScreen(),
    );
  }
}

// ==========================================
// 1. DATA MODEL
// ==========================================
class FlowModel {
  final String id;
  final String name;
  final String description;
  final String createdAt;

  FlowModel({
    required this.id,
    required this.name,
    required this.description,
    required this.createdAt,
  });

  factory FlowModel.fromJson(Map<String, dynamic> json) {
    return FlowModel(
      id: json['id'],
      name: json['name'],
      description: json['description'],
      createdAt: json['created_at'],
    );
  }
}

// ==========================================
// 2. API SERVICE (Connects to C++ Crow Server)
// ==========================================
class ApiService {
  // Note: If testing on Android Emulator, change 127.0.0.1 to 10.0.2.2
  static const String baseUrl = 'http://127.0.0.1:8080/api/v1/flows';

  Future<List<FlowModel>> getFlows() async {
    final response = await http.get(Uri.parse(baseUrl));
    if (response.statusCode == 200) {
      List<dynamic> body = jsonDecode(response.body);
      return body.map((item) => FlowModel.fromJson(item)).toList();
    } else {
      throw Exception('Failed to load flows');
    }
  }

  Future<void> createFlow(String name, String description) async {
    final response = await http.post(
      Uri.parse(baseUrl),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({'name': name, 'description': description}),
    );
    if (response.statusCode != 201) {
      throw Exception('Failed to create flow');
    }
  }

  Future<void> updateFlow(String id, String name, String description) async {
    final response = await http.put(
      Uri.parse('$baseUrl/$id'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({'name': name, 'description': description}),
    );
    if (response.statusCode != 200) {
      throw Exception('Failed to update flow');
    }
  }

  Future<void> deleteFlow(String id) async {
    final response = await http.delete(Uri.parse('$baseUrl/$id'));
    if (response.statusCode != 204) {
      throw Exception('Failed to delete flow');
    }
  }
}

// ==========================================
// 3. USER INTERFACE
// ==========================================
class FlowListScreen extends StatefulWidget {
  const FlowListScreen({super.key});

  @override
  State<FlowListScreen> createState() => _FlowListScreenState();
}

class _FlowListScreenState extends State<FlowListScreen> {
  final ApiService _apiService = ApiService();
  late Future<List<FlowModel>> _flowsFuture;

  @override
  void initState() {
    super.initState();
    _refreshFlows();
  }

  void _refreshFlows() {
    setState(() {
      _flowsFuture = _apiService.getFlows();
    });
  }

  // Dialog for both CREATE and UPDATE
  void _showFlowDialog({FlowModel? flow}) {
    final isEditing = flow != null;
    final nameController = TextEditingController(text: flow?.name ?? '');
    final descController = TextEditingController(text: flow?.description ?? '');

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(isEditing ? 'Edit Flow' : 'New Flow'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: nameController,
              decoration: const InputDecoration(labelText: 'Name'),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: descController,
              decoration: const InputDecoration(labelText: 'Description'),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () async {
              final name = nameController.text.trim();
              final desc = descController.text.trim();
              if (name.isEmpty || desc.isEmpty) return;

              Navigator.pop(context); // Close dialog

              try {
                if (isEditing) {
                  await _apiService.updateFlow(flow.id, name, desc);
                } else {
                  await _apiService.createFlow(name, desc);
                }
                _refreshFlows();
              } catch (e) {
                if (mounted) {
                  ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(content: Text('Error: $e')),
                  );
                }
              }
            },
            child: const Text('Save'),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('PostgreSQL Flows Editor'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: _refreshFlows,
            tooltip: 'Refresh Data',
          ),
        ],
      ),
      body: FutureBuilder<List<FlowModel>>(
        future: _flowsFuture,
        builder: (context, snapshot) {
          if (snapshot.connectionState == ConnectionState.waiting) {
            return const Center(child: CircularProgressIndicator());
          } else if (snapshot.hasError) {
            return Center(child: Text('Error: ${snapshot.error}'));
          } else if (!snapshot.hasData || snapshot.data!.isEmpty) {
            return const Center(child: Text('No rows found. Click + to add one.'));
          }

          final flows = snapshot.data!;
          return ListView.separated(
            padding: const EdgeInsets.all(16),
            itemCount: flows.length,
            separatorBuilder: (_, __) => const Divider(),
            itemBuilder: (context, index) {
              final flow = flows[index];
              return ListTile(
                title: Text(flow.name, style: const TextStyle(fontWeight: FontWeight.bold)),
                subtitle: Text('${flow.description}\nID: ${flow.id}'),
                isThreeLine: true,
                trailing: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    IconButton(
                      icon: const Icon(Icons.edit, color: Colors.blue),
                      onPressed: () => _showFlowDialog(flow: flow),
                    ),
                    IconButton(
                      icon: const Icon(Icons.delete, color: Colors.red),
                      onPressed: () async {
                        await _apiService.deleteFlow(flow.id);
                        _refreshFlows();
                      },
                    ),
                  ],
                ),
              );
            },
          );
        },
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () => _showFlowDialog(),
        child: const Icon(Icons.add),
      ),
    );
  }
}
