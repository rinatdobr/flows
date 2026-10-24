import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:intl/intl.dart';

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
  final String? startAt;
  final String? endAt;

  FlowModel({
    required this.id,
    required this.name,
    required this.description,
    required this.createdAt,
    this.startAt,
    this.endAt,
  });

  factory FlowModel.fromJson(Map<String, dynamic> json) {
    return FlowModel(
      id: json['id'],
      name: json['name'],
      description: json['description'],
      createdAt: json['created_at'],
      startAt: json['start_at'],
      endAt: json['end_at'],
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

  Future<void> createFlow(String name, String description, {String? startAt, String? endAt}) async {
    final response = await http.post(
      Uri.parse(baseUrl),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'name': name,
        'description': description,
        if (startAt != null && startAt.isNotEmpty) 'start_at': startAt,
        if (endAt != null && endAt.isNotEmpty) 'end_at': endAt,
      }),
    );
    if (response.statusCode != 201) {
      throw Exception('Failed to create flow');
    }
  }

  Future<void> updateFlow(String id, String name, String description, {String? startAt, String? endAt}) async {
    final response = await http.put(
      Uri.parse('$baseUrl/$id'),
      headers: {'Content-Type': 'application/json'},
      body: jsonEncode({
        'name': name,
        'description': description,
        if (startAt != null && startAt.isNotEmpty) 'start_at': startAt,
        if (endAt != null && endAt.isNotEmpty) 'end_at': endAt,
      }),
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
    DateTime? selectedStartDate;
    DateTime? selectedEndDate;

    // Parse existing dates if editing
    if (isEditing) {
      if (flow.startAt != null && flow.startAt!.isNotEmpty) {
        selectedStartDate = DateTime.parse(flow.startAt!);
      }
      if (flow.endAt != null && flow.endAt!.isNotEmpty) {
        selectedEndDate = DateTime.parse(flow.endAt!);
      }
    }

    showDialog(
      context: context,
      builder: (dialogContext) => AlertDialog(
        title: Text(isEditing ? 'Edit Flow' : 'New Flow'),
        content: SingleChildScrollView(
          child: Column(
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
              const SizedBox(height: 16),
              // Start datetime picker
              ListTile(
                leading: const Icon(Icons.calendar_today),
                title: Text(
                  selectedStartDate != null
                      ? _formatDateTime(selectedStartDate!)
                      : 'Select start datetime (optional)',
                ),
                trailing: const Icon(Icons.chevron_right),
                dense: true,
                onTap: () async {
                  final DateTime? picked = await showDatePicker(
                    context: dialogContext,
                    firstDate: DateTime(2000),
                    lastDate: DateTime(2100),
                    initialDate: selectedStartDate ?? DateTime.now(),
                  );
                  if (picked != null) {
                    final TimeOfDay? time = await showTimePicker(
                      context: dialogContext,
                      initialTime: TimeOfDay.fromDateTime(picked),
                    );
                    if (time != null) {
                      final combined = DateTime(
                        picked.year,
                        picked.month,
                        picked.day,
                        time.hour,
                        time.minute,
                      );
                      setState(() {
                        selectedStartDate = combined;
                      });
                    }
                  }
                },
              ),
              // End datetime picker
              ListTile(
                leading: const Icon(Icons.calendar_today),
                title: Text(
                  selectedEndDate != null
                      ? _formatDateTime(selectedEndDate!)
                      : 'Select end datetime (optional)',
                ),
                trailing: const Icon(Icons.chevron_right),
                dense: true,
                onTap: () async {
                  final DateTime? picked = await showDatePicker(
                    context: dialogContext,
                    firstDate: DateTime(2000),
                    lastDate: DateTime(2100),
                    initialDate: selectedEndDate ?? DateTime.now(),
                  );
                  if (picked != null) {
                    final TimeOfDay? time = await showTimePicker(
                      context: dialogContext,
                      initialTime: TimeOfDay.fromDateTime(picked),
                    );
                    if (time != null) {
                      final combined = DateTime(
                        picked.year,
                        picked.month,
                        picked.day,
                        time.hour,
                        time.minute,
                      );
                      setState(() {
                        selectedEndDate = combined;
                      });
                    }
                  }
                },
              ),
            ],
          ),
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

              // Validate: end_at can't be earlier than start_at but can be equal
              if (selectedStartDate != null && selectedEndDate != null) {
                if (selectedEndDate!.isBefore(selectedStartDate!)) {
                  ScaffoldMessenger.of(dialogContext).showSnackBar(
                    const SnackBar(content: Text('Error: End datetime must be the same or after start datetime')),
                  );
                  return;
                }
              }

              // Format dates for API (ISO 8601)
              final startAtStr = selectedStartDate != null
                  ? selectedStartDate!.toIso8601String()
                  : '';
              final endAtStr = selectedEndDate != null
                  ? selectedEndDate!.toIso8601String()
                  : '';

              Navigator.pop(context); // Close dialog

              try {
                if (isEditing) {
                  await _apiService.updateFlow(
                    flow.id,
                    name,
                    desc,
                    startAt: startAtStr.isNotEmpty ? startAtStr : null,
                    endAt: endAtStr.isNotEmpty ? endAtStr : null,
                  );
                } else {
                  await _apiService.createFlow(
                    name,
                    desc,
                    startAt: startAtStr.isNotEmpty ? startAtStr : null,
                    endAt: endAtStr.isNotEmpty ? endAtStr : null,
                  );
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

  String _formatDateTime(DateTime dt) {
    final formatter = DateFormat('yyyy-MM-dd HH:mm');
    return formatter.format(dt);
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
                subtitle: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(flow.description),
                    Text('ID: ${flow.id}'),
                    if (flow.startAt != null && flow.startAt!.isNotEmpty)
                      Text('Start: ${_formatDateTime(DateTime.parse(flow.startAt!))}'),
                    if (flow.endAt != null && flow.endAt!.isNotEmpty)
                      Text('End: ${_formatDateTime(DateTime.parse(flow.endAt!))}'),
                  ],
                ),
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
